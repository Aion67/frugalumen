/*
 * lcd1602.c — drive an HD44780 LCD through a PCF8574 I2C port expander.
 *
 * The expander gives us 8 output pins in one I2C byte:
 *   bit 0 = RS (0 command, 1 data)   bit 4..7 = D4..D7 (one nibble of data)
 *   bit 1 = RW (always 0 = write)
 *   bit 2 = E  (pulse high then low to latch the nibble)
 *   bit 3 = backlight
 * So each byte to the LCD is sent as two nibbles, and each nibble needs an
 * E pulse. That is why every command is three I2C writes per nibble.
 */
#include <string.h>
#include "lcd1602.h"
#include "config.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "lcd";

#define BIT_RS         0x01
#define BIT_E          0x04
#define BIT_BACKLIGHT  0x08

#define CMD_CLEAR        0x01
#define CMD_ENTRY_MODE   0x06   /* cursor moves right, no display shift */
#define CMD_DISPLAY_ON   0x0C   /* display on, cursor off, blink off */
#define CMD_FUNCTION_SET 0x28   /* 4-bit bus, 2 lines, 5x8 font */
#define CMD_SET_DDRAM    0x80

static i2c_master_dev_handle_t s_dev;

static void expander_write(uint8_t value)
{
    i2c_master_transmit(s_dev, &value, 1, 50);
}

static void send_nibble(uint8_t nibble, bool is_data)
{
    uint8_t byte = (nibble & 0xF0) | BIT_BACKLIGHT;
    if (is_data) {
        byte |= BIT_RS;
    }
    expander_write(byte | BIT_E);    /* E high: LCD samples the data lines */
    esp_rom_delay_us(1);
    expander_write(byte);            /* E low */
    esp_rom_delay_us(50);            /* most commands need ~37 us to finish */
}

static void send_byte(uint8_t value, bool is_data)
{
    send_nibble(value & 0xF0, is_data);
    send_nibble((uint8_t)(value << 4), is_data);
}

static void command(uint8_t cmd)
{
    send_byte(cmd, false);
}

esp_err_t lcd1602_init(i2c_master_bus_handle_t bus)
{
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = I2C_ADDR_LCD,
        .scl_speed_hz    = I2C_SPEED_HZ,
    };
    esp_err_t err = i2c_master_bus_add_device(bus, &cfg, &s_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "could not add LCD to I2C bus");
        return err;
    }

    /* HD44780 power-on dance: it wakes up in 8-bit mode, we must say "8-bit"
     * three times before it will accept the switch to 4-bit. */
    vTaskDelay(pdMS_TO_TICKS(50));
    send_nibble(0x30, false);
    vTaskDelay(pdMS_TO_TICKS(5));
    send_nibble(0x30, false);
    esp_rom_delay_us(150);
    send_nibble(0x30, false);
    esp_rom_delay_us(150);
    send_nibble(0x20, false);        /* now in 4-bit mode */
    esp_rom_delay_us(150);

    command(CMD_FUNCTION_SET);
    command(CMD_DISPLAY_ON);
    lcd1602_clear();
    command(CMD_ENTRY_MODE);
    return ESP_OK;
}

void lcd1602_clear(void)
{
    command(CMD_CLEAR);
    vTaskDelay(pdMS_TO_TICKS(2));    /* clear is the one slow command (~1.5 ms) */
}

void lcd1602_write_line(int row, const char *text)
{
    uint8_t address = (row == 0) ? 0x00 : 0x40;   /* line 2 starts at DDRAM 0x40 */
    command(CMD_SET_DDRAM | address);

    for (int i = 0; i < 16; i++) {
        char c = ' ';
        if (text[i] == '\0') {
            text = "";               /* past the end: keep padding with spaces */
        } else {
            c = text[i];
        }
        send_byte((uint8_t)c, true);
    }
}
