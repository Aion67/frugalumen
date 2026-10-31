/*
 * rtc_ds1307.c — read and write the DS1307 real-time clock.
 *
 * The chip keeps seven registers starting at address 0x00:
 *   seconds, minutes, hours, weekday, day, month, year
 * Every value is BCD (two decimal digits in one byte), so 0x59 means 59.
 */
#include "rtc_ds1307.h"
#include "config.h"
#include "esp_log.h"

static const char *TAG = "rtc";

static i2c_master_dev_handle_t s_dev;

static uint8_t bcd_to_bin(uint8_t bcd)
{
    return (bcd >> 4) * 10 + (bcd & 0x0F);
}

static uint8_t bin_to_bcd(uint8_t bin)
{
    return ((bin / 10) << 4) | (bin % 10);
}

esp_err_t rtc_ds1307_init(i2c_master_bus_handle_t bus)
{
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = I2C_ADDR_RTC,
        .scl_speed_hz    = I2C_SPEED_HZ,
    };
    esp_err_t err = i2c_master_bus_add_device(bus, &cfg, &s_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "could not add DS1307 to I2C bus");
        return err;
    }
    return ESP_OK;
}

esp_err_t rtc_ds1307_read(time_t *out)
{
    uint8_t start_reg = 0x00;
    uint8_t regs[7];
    esp_err_t err = i2c_master_transmit_receive(s_dev, &start_reg, 1, regs, sizeof regs, 100);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "RTC read failed (is the DS1307 wired?)");
        return err;
    }

    struct tm t = { 0 };
    t.tm_sec  = bcd_to_bin(regs[0] & 0x7F);   /* bit 7 is the clock-halt flag */
    t.tm_min  = bcd_to_bin(regs[1]);
    t.tm_hour = bcd_to_bin(regs[2] & 0x3F);   /* we always use 24-hour mode */
    t.tm_mday = bcd_to_bin(regs[4]);
    t.tm_mon  = bcd_to_bin(regs[5]) - 1;      /* struct tm months are 0..11 */
    t.tm_year = bcd_to_bin(regs[6]) + 100;    /* DS1307 stores 00..99 = 2000..2099 */

    *out = mktime(&t);
    return ESP_OK;
}

esp_err_t rtc_ds1307_write(time_t now)
{
    struct tm t;
    localtime_r(&now, &t);

    uint8_t frame[8];
    frame[0] = 0x00;                            /* start writing at register 0 */
    frame[1] = bin_to_bcd(t.tm_sec);            /* clock-halt bit left at 0 = running */
    frame[2] = bin_to_bcd(t.tm_min);
    frame[3] = bin_to_bcd(t.tm_hour);
    frame[4] = bin_to_bcd(t.tm_wday + 1);
    frame[5] = bin_to_bcd(t.tm_mday);
    frame[6] = bin_to_bcd(t.tm_mon + 1);
    frame[7] = bin_to_bcd(t.tm_year - 100);

    esp_err_t err = i2c_master_transmit(s_dev, frame, sizeof frame, 100);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "RTC write failed");
        return err;
    }
    return ESP_OK;
}
