#pragma once
/* Private to rfid_door: short feedback tones. Each call blocks for under half a second. */
#include "esp_err.h"

esp_err_t buzzer_init(void);
void      buzzer_beep_ok(void);     /* one short high beep   */
void      buzzer_beep_deny(void);   /* two low beeps         */
