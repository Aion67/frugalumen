#pragma once
#include "esp_err.h"
#include "core.h"
#include "driver/gpio.h"

esp_err_t lighting_init(room_id_t room, gpio_num_t pir_pin, gpio_num_t led_pin);
