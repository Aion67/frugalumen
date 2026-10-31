/*
 * Lesson 04 — read a card's UID, step by step.
 *
 * The MFRC522 does the radio work; we drive it through registers:
 *   1. put bytes for the card in the FIFO
 *   2. start the TRANSCEIVE command
 *   3. wait for the RX interrupt flag (or timeout)
 *   4. read the card's reply back out of the FIFO
 *
 * Two card commands get us the UID:
 *   REQA     0x26        (7-bit frame!)  -> card answers 2 bytes (ATQA)
 *   ANTICOLL 0x93 0x20                   -> card answers UID0..3 + BCC
 * BCC is a check byte: UID0 ^ UID1 ^ UID2 ^ UID3.
 *
 * Wokwi: click the reader, pick a card, press Tap.
 */
#include <stdio.h>
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "config.h"

static const char *TAG = "lesson04";

/* register addresses from the MFRC522 datasheet */
#define REG_COMMAND     0x01
#define REG_COM_IRQ     0x04
#define REG_ERROR       0x06
#define REG_FIFO_DATA   0x09
#define REG_FIFO_LEVEL  0x0A
#define REG_BIT_FRAMING 0x0D
#define REG_COLL        0x0E
#define REG_MODE        0x11
#define REG_TX_CONTROL  0x14
#define REG_TX_ASK      0x15
#define REG_T_MODE      0x2A
#define REG_T_PRESCALER 0x2B
#define REG_T_RELOAD_H  0x2C
#define REG_T_RELOAD_L  0x2D

static spi_device_handle_t s_spi;

static void write_reg(uint8_t reg, uint8_t value)
{
    uint8_t tx[2] = { (reg << 1) & 0x7E, value };     /* bit7 = 0 means write */
    spi_transaction_t t = { .length = 16, .tx_buffer = tx };
    spi_device_transmit(s_spi, &t);
}

static uint8_t read_reg(uint8_t reg)
{
    uint8_t tx[2] = { ((reg << 1) & 0x7E) | 0x80, 0 };  /* bit7 = 1 means read */
    uint8_t rx[2] = { 0, 0 };
    spi_transaction_t t = { .length = 16, .tx_buffer = tx, .rx_buffer = rx };
    spi_device_transmit(s_spi, &t);
    return rx[1];
}

/* Send tx bytes to the card, get the reply. Returns reply length, 0 if no card. */
static int transceive(const uint8_t *tx, int tx_len, uint8_t *rx, int rx_max, uint8_t bit_framing)
{
    write_reg(REG_COMMAND, 0x00);                 /* Idle: stop anything running */
    write_reg(REG_COM_IRQ, 0x7F);                 /* clear interrupt flags */
    write_reg(REG_FIFO_LEVEL, 0x80);              /* flush the FIFO */
    for (int i = 0; i < tx_len; i++) {
        write_reg(REG_FIFO_DATA, tx[i]);          /* load the bytes for the card */
    }
    write_reg(REG_BIT_FRAMING, bit_framing);      /* 7 = last byte is only 7 bits (REQA) */
    write_reg(REG_COMMAND, 0x0C);                 /* Transceive */
    write_reg(REG_BIT_FRAMING, bit_framing | 0x80); /* StartSend */

    bool got_reply = false;
    for (int ms = 0; ms < 40; ms++) {
        uint8_t irq = read_reg(REG_COM_IRQ);
        if (irq & 0x30) {                         /* RxIRq or IdleIRq: a reply is in */
            got_reply = true;
            break;
        }
        if (irq & 0x01) {                         /* TimerIRq: the chip gave up */
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    write_reg(REG_BIT_FRAMING, bit_framing);      /* clear StartSend */
    if (!got_reply || (read_reg(REG_ERROR) & 0x13)) {
        return 0;
    }
    int n = read_reg(REG_FIFO_LEVEL);
    if (n > rx_max) {
        n = rx_max;
    }
    for (int i = 0; i < n; i++) {
        rx[i] = read_reg(REG_FIFO_DATA);
    }
    return n;
}

void app_main(void)
{
    gpio_config_t rst = { .pin_bit_mask = 1ULL << PIN_RFID_RST, .mode = GPIO_MODE_OUTPUT };
    ESP_ERROR_CHECK(gpio_config(&rst));
    gpio_set_level(PIN_RFID_RST, 1);

    spi_bus_config_t bus = {
        .sclk_io_num = PIN_SPI_SCK, .mosi_io_num = PIN_SPI_MOSI, .miso_io_num = PIN_SPI_MISO,
        .quadwp_io_num = -1, .quadhd_io_num = -1,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));
    spi_device_interface_config_t dev = {
        .clock_speed_hz = 1000000, .mode = 0, .spics_io_num = PIN_RFID_ENTRY_CS, .queue_size = 1,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &dev, &s_spi));

    /* chip setup, same as the real driver */
    write_reg(REG_COMMAND, 0x0F);                 /* soft reset */
    vTaskDelay(pdMS_TO_TICKS(50));
    write_reg(REG_T_MODE, 0x80);                  /* timer: auto-start after transmit */
    write_reg(REG_T_PRESCALER, 0xA9);
    write_reg(REG_T_RELOAD_H, 0x03);
    write_reg(REG_T_RELOAD_L, 0xE8);              /* ~25 ms timeout */
    write_reg(REG_TX_ASK, 0x40);                  /* 100% ASK */
    write_reg(REG_MODE, 0x3D);                    /* CRC preset */
    write_reg(REG_TX_CONTROL, read_reg(REG_TX_CONTROL) | 0x03);   /* antenna on */
    ESP_LOGI(TAG, "reader ready - tap a card in Wokwi");

    while (1) {
        uint8_t reply[8];
        write_reg(REG_COLL, read_reg(REG_COLL) & 0x7F);
        uint8_t reqa = 0x26;
        if (transceive(&reqa, 1, reply, sizeof reply, 0x07) == 2) {
            uint8_t anticoll[2] = { 0x93, 0x20 };
            if (transceive(anticoll, 2, reply, sizeof reply, 0x00) == 5) {
                uint8_t bcc = reply[0] ^ reply[1] ^ reply[2] ^ reply[3];
                ESP_LOGI(TAG, "card UID %02X:%02X:%02X:%02X  (BCC %s)",
                         reply[0], reply[1], reply[2], reply[3], bcc == reply[4] ? "ok" : "BAD");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(RFID_POLL_MS));
    }
}
