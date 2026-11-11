/* Test harness for storage. Owner: hairat. Keep this small; it exists to prove the module works alone. */
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "core.h"
#include "config.h"
#include "storage.h"

static const char *TAG = "sandbox";

// Prints the last 10 events found on the card
static void show_last_rows(void)
{
    event_t rows[10];
    int n = storage_read_last(10, rows);
    ESP_LOGI(TAG, "Rows read back: %d", n);
    for (int i = 0; i < n; i++) {
        ESP_LOGI(TAG, "ts=%lld type=%d uid=%lu room=%d note=%s",
                 (long long)rows[i].ts, (int)rows[i].type,
                 (unsigned long)rows[i].uid, (int)rows[i].room, rows[i].note);
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(core_init());

    // storage_init() takes no arguments (see storage.h)
    if (storage_init() != ESP_OK) {
        ESP_LOGE(TAG, "storage_init failed");
    }
    ESP_LOGI(TAG, "storage sandbox running");

    // 1. show what is already on the card (proves events survive a reset)
    show_last_rows();

    // 2. send three fake events, the logger task saves them
    for (int i = 0; i < 3; i++) {
        event_t e = {0};
        e.ts = 1761300000 + i;
        e.type = (i == 0) ? EV_ENTRY : (i == 1) ? EV_DENIED : EV_EXIT;
        e.uid = 3405691582u + i;
        e.room = ROOM_COUNT;                // not a room event
        strcpy(e.note, "test, event");      // the comma gets cleaned by storage
        core_emit(&e);
    }

    vTaskDelay(pdMS_TO_TICKS(1000));        // give the logger time to write

    // 3. read them back and count by type
    show_last_rows();
    ESP_LOGI(TAG, "EV_ENTRY count: %d", storage_count(EV_ENTRY));
    ESP_LOGI(TAG, "EV_DENIED count: %d", storage_count(EV_DENIED));

    for (;;) {
        ESP_LOGI(TAG, "people inside: %d", core_people_inside());
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}