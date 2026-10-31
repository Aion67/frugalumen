/*
 * Lesson 04 — the real storage component.
 *
 * storage_init() mounts the card and starts the logger task, which blocks on
 * core's event queue. We emit a few events from app_main, wait, then use
 * storage_read_last() and storage_count() like the console does.
 * With the card removed in Wokwi it still works, from the RAM ring buffer.
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "core.h"
#include "config.h"
#include "storage.h"

static const char *TAG = "lesson04";

static void emit(event_type_t type, uint32_t uid, const char *note)
{
    event_t e = { .ts = time(NULL), .type = type, .uid = uid, .room = ROOM_COUNT };
    strlcpy(e.note, note, sizeof e.note);
    core_emit(&e);
}

void app_main(void)
{
    ESP_ERROR_CHECK(core_init());
    ESP_ERROR_CHECK(storage_init());
    ESP_LOGI(TAG, "SD mounted: %s", storage_sd_mounted() ? "yes" : "no, RAM only");

    emit(EV_ENTRY,  0x01020304, "entry");
    emit(EV_ENTRY,  0x11223344, "entry");
    emit(EV_DENIED, 0xAABBCCDD, "entry: unknown tag");
    emit(EV_EXIT,   0x01020304, "exit");

    vTaskDelay(pdMS_TO_TICKS(1000));   /* give the logger task time to write */

    event_t last[10];
    int n = storage_read_last(10, last);
    ESP_LOGI(TAG, "last %d events on record:", n);
    for (int i = 0; i < n; i++) {
        ESP_LOGI(TAG, "  %-7s %08lX %s", core_event_name(last[i].type),
                 (unsigned long)last[i].uid, last[i].note);
    }
    ESP_LOGI(TAG, "entries ever logged: %d (grows across resets when the card is present)",
             storage_count(EV_ENTRY));
}
