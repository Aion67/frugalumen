#pragma once
/* Private to core: tiny driver for the DS1307 real-time clock (I2C, address 0x68). */
#include <time.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

esp_err_t rtc_ds1307_init(i2c_master_bus_handle_t bus);
esp_err_t rtc_ds1307_read(time_t *out);
esp_err_t rtc_ds1307_write(time_t now);
