#define LOG_LOCAL_LEVEL ESP_LOG_INFO

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "esp_log.h"

#define ENTRY_CS    GPIO_NUM_5
#define ENTRY_RST   GPIO_NUM_22
#define EXIT_CS     GPIO_NUM_21
#define EXIT_RST    GPIO_NUM_4

#define SPI_SCK     GPIO_NUM_18
#define SPI_MOSI    GPIO_NUM_23
#define SPI_MISO    GPIO_NUM_19

#define ENTRY_SERVO GPIO_NUM_26
#define EXIT_SERVO  GPIO_NUM_25
#define BUZZER_PIN  GPIO_NUM_27

/* Servo PWM settings */
#define SERVO_FREQUENCY  50
#define SERVO_RESOLUTION LEDC_TIMER_14_BIT
#define SERVO_MAX_DUTY   16383
#define SERVO_CLOSED_DEG 0
#define SERVO_OPEN_DEG   90
#define DOOR_OPEN_MS     2000

/* MFRC522 registers */
#define COMMAND_REG      0x01
#define COM_IRQ_REG      0x04
#define ERROR_REG        0x06
#define FIFO_DATA_REG    0x09
#define FIFO_LEVEL_REG   0x0A
#define BIT_FRAMING_REG  0x0D
#define MODE_REG         0x11
#define TX_CONTROL_REG   0x14
#define TX_ASK_REG       0x15
#define T_MODE_REG       0x2A
#define T_PRESCALER_REG  0x2B
#define T_RELOAD_H_REG   0x2C
#define T_RELOAD_L_REG   0x2D
#define VERSION_REG      0x37

#define PCD_IDLE       0x00
#define PCD_TRANSCEIVE 0x0C
#define PCD_SOFT_RESET 0x0F

#define PICC_REQA     0x26
#define PICC_ANTICOLL 0x93

#define UID_LENGTH        4
#define MISSES_TO_REMOVE  3

static const char *TAG = "RFID_DOOR";

typedef struct {
    const char *name;
    gpio_num_t cs_pin;
    spi_device_handle_t spi;
} rfid_reader_t;

typedef struct {
    uint8_t last_uid[UID_LENGTH];
    bool has_last_uid;
    uint8_t missed_reads;
} card_state_t;

/* Blue Card and Green Card are authorized */
static const uint8_t authorized_cards[][UID_LENGTH] = {
    {0x01, 0x02, 0x03, 0x04},
    {0x11, 0x22, 0x33, 0x44}
};

#define AUTHORIZED_CARD_COUNT \
    (sizeof(authorized_cards) / sizeof(authorized_cards[0]))

/* Anti-passback: false = outside, true = inside */
static bool card_inside[AUTHORIZED_CARD_COUNT] = {false};

/* ---------- SERVO CONTROL ---------- */

static void set_servo_angle(ledc_channel_t channel, int angle)
{
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;

    /*
     * Map angle to a pulse between 500 and 2400 microseconds.
     * At 50 Hz, one PWM period is 20,000 microseconds.
     */
    uint32_t pulse_us =
        500 + ((uint32_t)angle * 1900) / 180;

    uint32_t duty =
        (pulse_us * SERVO_MAX_DUTY) / 20000;

    ESP_ERROR_CHECK(ledc_set_duty(
        LEDC_LOW_SPEED_MODE, channel, duty));

    ESP_ERROR_CHECK(ledc_update_duty(
        LEDC_LOW_SPEED_MODE, channel));
}

static void initialize_servos(void)
{
    ledc_timer_config_t timer_config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = SERVO_RESOLUTION,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = SERVO_FREQUENCY,
        .clk_cfg = LEDC_AUTO_CLK
    };

    ESP_ERROR_CHECK(ledc_timer_config(&timer_config));

    ledc_channel_config_t entry_config = {
        .gpio_num = ENTRY_SERVO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0
    };

    ledc_channel_config_t exit_config = {
        .gpio_num = EXIT_SERVO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_1,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0
    };

    ESP_ERROR_CHECK(ledc_channel_config(&entry_config));
    ESP_ERROR_CHECK(ledc_channel_config(&exit_config));

    set_servo_angle(LEDC_CHANNEL_0, SERVO_CLOSED_DEG);
    set_servo_angle(LEDC_CHANNEL_1, SERVO_CLOSED_DEG);

    ESP_LOGI(TAG, "Servos initialized; both doors closed");
}

static void open_door(ledc_channel_t channel,
                      const char *door_name)
{
    ESP_LOGI(TAG, "%s door opening", door_name);

    set_servo_angle(channel, SERVO_OPEN_DEG);
    vTaskDelay(pdMS_TO_TICKS(DOOR_OPEN_MS));

    set_servo_angle(channel, SERVO_CLOSED_DEG);

    ESP_LOGI(TAG, "%s door closed", door_name);
}

/* ---------- RFID COMMUNICATION ---------- */

static uint8_t rfid_read_register(rfid_reader_t *reader,
                                  uint8_t reg)
{
    uint8_t tx_data[2] = {
        (uint8_t)(((reg << 1) & 0x7E) | 0x80), 0
    };
    uint8_t rx_data[2] = {0};

    spi_transaction_t transaction = {
        .length = 16,
        .tx_buffer = tx_data,
        .rx_buffer = rx_data
    };

    gpio_set_level(reader->cs_pin, 0);
    esp_err_t result =
        spi_device_transmit(reader->spi, &transaction);
    gpio_set_level(reader->cs_pin, 1);

    if (result != ESP_OK) return 0;

    return rx_data[1];
}

static void rfid_write_register(rfid_reader_t *reader,
                                uint8_t reg, uint8_t value)
{
    uint8_t tx_data[2] = {
        (uint8_t)((reg << 1) & 0x7E), value
    };

    spi_transaction_t transaction = {
        .length = 16,
        .tx_buffer = tx_data
    };

    gpio_set_level(reader->cs_pin, 0);
    spi_device_transmit(reader->spi, &transaction);
    gpio_set_level(reader->cs_pin, 1);
}

static bool rfid_read_fifo(rfid_reader_t *reader,
                           uint8_t *data, uint8_t count)
{
    if (count == 0 || count > 64) return false;

    uint8_t tx_data[65] = {0};
    uint8_t rx_data[65] = {0};

    tx_data[0] =
        (uint8_t)(((FIFO_DATA_REG << 1) & 0x7E) | 0x80);

    spi_transaction_t transaction = {
        .length = (count + 1) * 8,
        .tx_buffer = tx_data,
        .rx_buffer = rx_data
    };

    gpio_set_level(reader->cs_pin, 0);
    esp_err_t result =
        spi_device_transmit(reader->spi, &transaction);
    gpio_set_level(reader->cs_pin, 1);

    if (result != ESP_OK) return false;

    for (uint8_t i = 0; i < count; i++) {
        data[i] = rx_data[i + 1];
    }

    return true;
}

static bool rfid_transceive(rfid_reader_t *reader,
                            const uint8_t *send_data,
                            uint8_t send_length,
                            uint8_t *receive_data,
                            uint8_t *receive_length)
{
    rfid_write_register(reader, COMMAND_REG, PCD_IDLE);
    rfid_write_register(reader, COM_IRQ_REG, 0x7F);
    rfid_write_register(reader, FIFO_LEVEL_REG, 0x80);

    for (uint8_t i = 0; i < send_length; i++) {
        rfid_write_register(reader, FIFO_DATA_REG,
                            send_data[i]);
    }

    rfid_write_register(reader, COMMAND_REG, PCD_TRANSCEIVE);

    uint8_t framing =
        rfid_read_register(reader, BIT_FRAMING_REG);

    rfid_write_register(reader, BIT_FRAMING_REG,
                        framing | 0x80);

    bool received = false;

    for (int i = 0; i < 2000; i++) {
        uint8_t irq =
            rfid_read_register(reader, COM_IRQ_REG);

        if (irq & 0x20) {
            received = true;
            break;
        }

        if (irq & 0x01) break;

        vTaskDelay(pdMS_TO_TICKS(1));
    }

    framing = rfid_read_register(reader, BIT_FRAMING_REG);
    rfid_write_register(reader, BIT_FRAMING_REG,
                        framing & 0x7F);

    if (!received) return false;

    if (rfid_read_register(reader, ERROR_REG) & 0x1B) {
        return false;
    }

    uint8_t count =
        rfid_read_register(reader, FIFO_LEVEL_REG);

    if (count == 0 || count > 64) return false;

    if (!rfid_read_fifo(reader, receive_data, count)) {
        return false;
    }

    *receive_length = count;
    return true;
}

static bool setup_reader(rfid_reader_t *reader,
                         const char *name,
                         gpio_num_t cs_pin,
                         gpio_num_t rst_pin)
{
    reader->name = name;
    reader->cs_pin = cs_pin;
    reader->spi = NULL;

    gpio_config_t config = {
        .pin_bit_mask = 1ULL << cs_pin,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    if (gpio_config(&config) != ESP_OK) return false;

    gpio_set_level(cs_pin, 1);

    config.pin_bit_mask = 1ULL << rst_pin;

    if (gpio_config(&config) != ESP_OK) return false;

    gpio_set_level(rst_pin, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(rst_pin, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    spi_device_interface_config_t device_config = {
        .clock_speed_hz = 1000000,
        .mode = 0,
        .spics_io_num = -1,
        .queue_size = 1
    };

    if (spi_bus_add_device(SPI2_HOST, &device_config,
                           &reader->spi) != ESP_OK) {
        ESP_LOGE(TAG, "%s: SPI setup failed", name);
        return false;
    }

    rfid_write_register(reader, COMMAND_REG, PCD_SOFT_RESET);
    vTaskDelay(pdMS_TO_TICKS(50));

    rfid_write_register(reader, T_MODE_REG, 0x8D);
    rfid_write_register(reader, T_PRESCALER_REG, 0x3E);
    rfid_write_register(reader, T_RELOAD_L_REG, 30);
    rfid_write_register(reader, T_RELOAD_H_REG, 0);
    rfid_write_register(reader, TX_ASK_REG, 0x40);
    rfid_write_register(reader, MODE_REG, 0x3D);

    uint8_t tx_control =
        rfid_read_register(reader, TX_CONTROL_REG);

    if ((tx_control & 0x03) != 0x03) {
        rfid_write_register(reader, TX_CONTROL_REG,
                            tx_control | 0x03);
    }

    uint8_t version = rfid_read_register(reader, VERSION_REG);

    ESP_LOGI(TAG, "%s reader version: 0x%02X",
             name, version);

    return version != 0x00 && version != 0xFF;
}

static bool read_card_uid(rfid_reader_t *reader,
                          uint8_t uid[UID_LENGTH])
{
    uint8_t request = PICC_REQA;
    uint8_t response[10] = {0};
    uint8_t length = 0;

    rfid_write_register(reader, BIT_FRAMING_REG, 0x07);

    if (!rfid_transceive(reader, &request, 1,
                         response, &length) ||
        length != 2) {
        return false;
    }

    uint8_t anticollision[2] = {PICC_ANTICOLL, 0x20};

    rfid_write_register(reader, BIT_FRAMING_REG, 0x00);
    length = 0;

    if (!rfid_transceive(reader, anticollision, 2,
                         response, &length) ||
        length != 5) {
        return false;
    }

    uint8_t checksum = response[0] ^ response[1]
                     ^ response[2] ^ response[3];

    if (checksum != response[4]) return false;

    memcpy(uid, response, UID_LENGTH);
    return true;
}

/* ---------- AUTHORIZATION AND ANTI-PASSBACK ---------- */

static int find_authorized_card(const uint8_t uid[UID_LENGTH])
{
    for (size_t i = 0; i < AUTHORIZED_CARD_COUNT; i++) {
        if (memcmp(uid, authorized_cards[i], UID_LENGTH) == 0) {
            return (int)i;
        }
    }

    return -1;
}

static void process_reader(rfid_reader_t *reader,
                           card_state_t *state,
                           ledc_channel_t servo_channel,
                           bool is_entry_reader)
{
    uint8_t uid[UID_LENGTH];

    if (read_card_uid(reader, uid)) {
        state->missed_reads = 0;

        bool same_card = state->has_last_uid &&
            memcmp(uid, state->last_uid, UID_LENGTH) == 0;

        if (same_card) return;

        memcpy(state->last_uid, uid, UID_LENGTH);
        state->has_last_uid = true;

        ESP_LOGI(TAG, "%s: Card UID %02X:%02X:%02X:%02X",
                 reader->name,
                 uid[0], uid[1], uid[2], uid[3]);

        int card_index = find_authorized_card(uid);

        if (card_index < 0) {
            ESP_LOGW(TAG,
                     "%s: ACCESS DENIED - unknown card",
                     reader->name);
            return;
        }

        if (is_entry_reader) {
            if (card_inside[card_index]) {
                ESP_LOGW(TAG,
                         "%s: ACCESS DENIED - card is already inside",
                         reader->name);
                return;
            }

            ESP_LOGI(TAG, "%s: ACCESS GRANTED - entry",
                     reader->name);
            open_door(servo_channel, reader->name);
            card_inside[card_index] = true;
            ESP_LOGI(TAG, "Card marked INSIDE");
        } else {
            if (!card_inside[card_index]) {
                ESP_LOGW(TAG,
                         "%s: ACCESS DENIED - card is not inside",
                         reader->name);
                return;
            }

            ESP_LOGI(TAG, "%s: ACCESS GRANTED - exit",
                     reader->name);
            open_door(servo_channel, reader->name);
            card_inside[card_index] = false;
            ESP_LOGI(TAG, "Card marked OUTSIDE");
        }

        return;
    }

    if (state->has_last_uid) {
        state->missed_reads++;

        if (state->missed_reads >= MISSES_TO_REMOVE) {
            ESP_LOGI(TAG, "%s: Card removed", reader->name);
            state->has_last_uid = false;
            state->missed_reads = 0;
        }
    }
}

void app_main(void)
{
    esp_log_level_set(TAG, ESP_LOG_INFO);

    /* Start with both doors closed */
    initialize_servos();

    spi_bus_config_t bus_config = {
        .mosi_io_num = SPI_MOSI,
        .miso_io_num = SPI_MISO,
        .sclk_io_num = SPI_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 128
    };

    ESP_ERROR_CHECK(
        spi_bus_initialize(SPI2_HOST, &bus_config,
                           SPI_DMA_DISABLED)
    );

    rfid_reader_t entry_reader;
    rfid_reader_t exit_reader;

    if (!setup_reader(&entry_reader, "ENTRY",
                      ENTRY_CS, ENTRY_RST)) {
        ESP_LOGE(TAG, "Entry reader setup failed");
        return;
    }

    if (!setup_reader(&exit_reader, "EXIT",
                      EXIT_CS, EXIT_RST)) {
        ESP_LOGE(TAG, "Exit reader setup failed");
        return;
    }

    card_state_t entry_state = {0};
    card_state_t exit_state = {0};

    ESP_LOGI(TAG, "RFID readers and doors ready");

    while (1) {
        process_reader(&entry_reader, &entry_state,
                       LEDC_CHANNEL_0, true);

        process_reader(&exit_reader, &exit_state,
                       LEDC_CHANNEL_1, false);

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}