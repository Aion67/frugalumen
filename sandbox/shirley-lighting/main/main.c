/* Test harness for lighting. Owner: shirley. Keep this small; it exists to prove the module works alone. */
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "core.h"
#include "config.h"
#include "lighting.h"

static const char *TAG = "sandbox";

void app_main(void)
{
    ESP_ERROR_CHECK(core_init());
    /* TODO(shirley): call lighting_init(...) with the right arguments, then exercise it */
    ESP_LOGI(TAG, "lighting sandbox running");
    for (;;) {
        ESP_LOGI(TAG, "people inside: %d", core_people_inside());
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
