#pragma once
/* Private to display: HD44780 16x2 LCD behind a PCF8574 I2C backpack. */
#include "esp_err.h"
#include "driver/i2c_master.h"

esp_err_t lcd1602_init(i2c_master_bus_handle_t bus);
void      lcd1602_clear(void);
void      lcd1602_write_line(int row, const char *text);   /* pads/truncates to 16 chars */
