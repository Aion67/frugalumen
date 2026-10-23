#include "console.h"
#include "config.h"
#include "esp_log.h"

static const char *TAG = "console";

/* TODO(owner): real implementation. This stub only proves the module links. */
esp_err_t console_init(void) { ESP_LOGW(TAG, "stub"); return ESP_OK; }
