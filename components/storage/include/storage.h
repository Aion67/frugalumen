#pragma once
/* storage — the only writer of the microSD event log (requirement 2.4). */
#include "esp_err.h"
#include "core.h"

esp_err_t storage_init(void);

/* Copies the newest n events into out (oldest first). Returns how many were copied. */
int storage_read_last(int n, event_t *out);

/* Number of logged events of one type, e.g. storage_count(EV_ENTRY). */
int storage_count(event_type_t type);

/* True when the microSD card mounted. If false, the log lives only in RAM. */
bool storage_sd_mounted(void);
