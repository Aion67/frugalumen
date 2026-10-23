#include "esp_log.h"
#include "core.h"
#include "config.h"
#include "rfid_door.h"
#include "lighting.h"
#include "storage.h"
#include "console.h"
#include "display.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "frugalumen starting");
    ESP_ERROR_CHECK(core_init());
    ESP_ERROR_CHECK(storage_init());      // logger first so nothing is lost
    ESP_ERROR_CHECK(rfid_door_init());
    ESP_ERROR_CHECK(lighting_init(ROOM_1, PIN_PIR_ROOM1, PIN_LED_ROOM1));
    ESP_ERROR_CHECK(lighting_init(ROOM_2, PIN_PIR_ROOM2, PIN_LED_ROOM2));
    ESP_ERROR_CHECK(display_init());
    ESP_ERROR_CHECK(console_init());
    ESP_LOGI(TAG, "all modules up");
}
