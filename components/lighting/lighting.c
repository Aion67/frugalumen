#include "lighting.h"
#include "config.h"
#include "esp_log.h"

static const char *TAG = "lighting";

/* TODO(owner): real implementation. This stub only proves the module links. */
esp_err_t lighting_init(room_id_t room, gpio_num_t pir_pin, gpio_num_t led_pin)
{ (void)pir_pin; (void)led_pin; ESP_LOGW(TAG, "stub room %d", room); return ESP_OK; }
