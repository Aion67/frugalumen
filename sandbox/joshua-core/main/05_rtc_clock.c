/*
 * Lesson 05 — reading the DS1307 real-time clock over I2C.
 *
 * I2C is a two-wire bus (SDA data, SCL clock). Each chip has a 7-bit address;
 * the DS1307 is 0x68. To read it we WRITE one byte (the register number to
 * start at) and then READ seven bytes back. The new ESP-IDF driver does both
 * halves in one call: i2c_master_transmit_receive().
 *
 * Values are BCD: 0x45 means "45". Decode with (high nibble * 10 + low nibble).
 * In Wokwi set the DS1307 attribute "initTime" to "now" to get your PC's time.
 */
#include <stdio.h>
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "config.h"

static const char *TAG = "lesson05";

static int bcd_to_bin(uint8_t v)
{
    return (v >> 4) * 10 + (v & 0x0F); // watch out for arithmetic right shift
}

void app_main(void)
{
    /* Step 1: create the bus (which pins, which I2C controller). */
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port   = I2C_NUM_0,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus));

    /* Step 2: describe the device on that bus (address + speed). */
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = I2C_ADDR_RTC,
        .scl_speed_hz    = I2C_SPEED_HZ,
    };
    i2c_master_dev_handle_t rtc;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg, &rtc));

    while (1) {
        uint8_t start_reg = 0x00;
        uint8_t regs[7];
        esp_err_t err = i2c_master_transmit_receive(rtc, &start_reg, 1, regs, 7, 100);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "read failed: %s (check SDA=21, SCL=22 wiring)", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "20%02d-%02d-%02d %02d:%02d:%02d",
                     bcd_to_bin(regs[6]), bcd_to_bin(regs[5]), bcd_to_bin(regs[4]),
                     bcd_to_bin(regs[2] & 0x3F), bcd_to_bin(regs[1]), bcd_to_bin(regs[0] & 0x7F));
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
