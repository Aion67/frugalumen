#pragma once
/* rfid_door — the entrance terminal: two RFID readers, two doors, one buzzer (requirement 2.1). */
#include "esp_err.h"
#include "core.h"

esp_err_t rfid_door_init(void);
