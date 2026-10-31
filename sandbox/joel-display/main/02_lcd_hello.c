/*
 * Lesson 02 — put text on the LCD through the I2C backpack.
 *
 * The HD44780 LCD controller normally needs 11 pins. The PCF8574 backpack
 * gives us 8 pins over I2C, so the LCD runs in "4-bit mode": every byte is
 * sent as two halves (nibbles). Each nibble is latched by pulsing E high
 * then low. The backpack bit layout:
 *   bit0 RS (0 = command, 1 = character)   bit4..7 = D4..D7 data
 *   bit1 RW (always 0, write)              bit3 backlight on
 *   bit2 E  (the latch pulse)
 *
 * This file is the display's lcd1602.c with everything spelled out.
 */
#include <stdio.h>
#include <string.h>
#include "driver/i2c_master.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "config.h"

#define BIT_RS        0x01
#define BIT_E         0x04
#define BIT_BACKLIGHT 0x08

static i2c_master_dev_handle_t s_lcd;

static void expander_write(uint8_t v)
{
    i2c_master_transmit(s_lcd, &v, 1, 50);
}

/* send the top 4 bits of 'nibble' with an E pulse */
static void send_nibble(uint8_t nibble, bool is_data)
{
    uint8_t byte = (nibble & 0xF0) | BIT_BACKLIGHT;
    if (is_data) {
        byte |= BIT_RS;
    }
    expander_write(byte | BIT_E);   /* E high: LCD reads D4..D7 */
    esp_rom_delay_us(1);            /* microseconds: too short for vTaskDelay */
    expander_write(byte);           /* E low */
    esp_rom_delay_us(50);           /* give the LCD time to execute */
}

static void send_byte(uint8_t value, bool is_data)
{
    send_nibble(value, is_data);           /* high nibble first */
    send_nibble(value << 4, is_data);      /* then low nibble */
}

static void lcd_init(void)
{
    vTaskDelay(pdMS_TO_TICKS(50));          /* LCD power-up time */
    send_nibble(0x30, false);               /* "8-bit mode" x3: the reset dance */
    vTaskDelay(pdMS_TO_TICKS(5));
    send_nibble(0x30, false);
    esp_rom_delay_us(150);
    send_nibble(0x30, false);
    esp_rom_delay_us(150);
    send_nibble(0x20, false);               /* switch to 4-bit mode */
    esp_rom_delay_us(150);
    send_byte(0x28, false);                 /* function set: 4-bit, 2 lines, 5x8 */
    send_byte(0x0C, false);                 /* display on, cursor off */
    send_byte(0x01, false);                 /* clear */
    vTaskDelay(pdMS_TO_TICKS(2));           /* clear is slow (~1.5 ms) */
    send_byte(0x06, false);                 /* cursor moves right after each char */
}

static void lcd_print(int row, const char *text)
{
    send_byte(0x80 | (row == 0 ? 0x00 : 0x40), false);   /* set cursor: row 2 starts at 0x40 */
    for (int i = 0; i < 16; i++) {
        char c = (i < strlen(text)) ? text[i] : ' ';      /* pad to 16 so old text is erased */
        send_byte((uint8_t)c, true);
    }
}

void app_main(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0, .sda_io_num = PIN_I2C_SDA, .scl_io_num = PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT, .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = I2C_ADDR_LCD,
        .scl_speed_hz    = I2C_SPEED_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg, &s_lcd));

    lcd_init();
    lcd_print(0, "Hello CoCIS");
    int n = 0;
    while (1) {
        char line[24];
        snprintf(line, sizeof line, "count: %d", n++);
        lcd_print(1, line);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
