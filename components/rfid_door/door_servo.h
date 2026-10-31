#pragma once
/* Private to rfid_door: two door servos driven by the LEDC PWM peripheral. */
#include "esp_err.h"
#include "core.h"

esp_err_t door_servo_init(void);
void      door_servo_set(lane_t lane, bool open);
