#pragma once
/* lighting — one PIR sensor and one light per lecture room (requirement 2.2). */
#include "esp_err.h"
#include "core.h"
#include "driver/gpio.h"

/* Call once per room. Starts a task that owns that room's PIR and LED. */
esp_err_t lighting_init(room_id_t room, gpio_num_t pir_pin, gpio_num_t led_pin);
