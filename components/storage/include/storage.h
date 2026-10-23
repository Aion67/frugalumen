#pragma once
#include "esp_err.h"
#include "core.h"
#include "driver/gpio.h"

esp_err_t storage_init(void);
int       storage_read_last(int n, event_t *out);
int       storage_count(event_type_t type);
