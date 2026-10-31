/*
 * Lesson 01 — scan the I2C bus and list who answers.
 *
 * I2C devices each have a 7-bit address. i2c_master_probe() sends just the
 * address and checks whether something ACKs. Expected on our bus:
 *   0x27  the LCD's PCF8574 backpack
 *   0x68  the DS1307 clock
 * If the LCD shows up at 0x3F instead, change I2C_ADDR_LCD in config.h.
 */
#include <stdio.h>
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "config.h"

static const char *TAG = "lesson01";

void app_main(void)
{
    i2c_master_bus_config_t cfg = {
        .i2c_port   = I2C_NUM_0,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&cfg, &bus));

    ESP_LOGI(TAG, "scanning 0x08..0x77");
    int found = 0;
    for (int addr = 0x08; addr <= 0x77; addr++) {
        if (i2c_master_probe(bus, addr, 20) == ESP_OK) {
            ESP_LOGI(TAG, "  device at 0x%02X", addr);
            found++;
        }
    }
    ESP_LOGI(TAG, "%d device(s) found", found);
}
