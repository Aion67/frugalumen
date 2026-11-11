#pragma once
/* Private to console: admin PIN kept in NVS so it survives a reset. */
#include <stdbool.h>
#include "esp_err.h"

esp_err_t auth_init(void);
bool      auth_check_pin(const char *pin);
esp_err_t auth_set_pin(const char *new_pin);
