#include "storage.h"
#include "config.h"
#include "esp_log.h"

static const char *TAG = "storage";

/* TODO(owner): real implementation. This stub only proves the module links. */
esp_err_t storage_init(void) { ESP_LOGW(TAG, "stub"); return ESP_OK; }
int storage_read_last(int n, event_t *out) { (void)n; (void)out; return 0; }
int storage_count(event_type_t type) { (void)type; return 0; }
