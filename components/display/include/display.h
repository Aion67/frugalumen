#pragma once
/* display — 16x2 LCD status board, refreshed once a second (requirement 2.5). */
#include "esp_err.h"
#include "core.h"

esp_err_t display_init(void);
