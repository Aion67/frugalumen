/*
 * Lesson 05 — the real entrance terminal.
 *
 * rfid_door_init() starts one task that polls both readers, checks each tag
 * against core (known? window open? anti-passback?), opens the right servo
 * for DOOR_OPEN_MS and beeps. We only start core, the module, and a task that
 * prints every event so you can watch the decisions.
 *
 * Try in Wokwi:
 *   blue card on ENTRY reader  -> ENTRY, door opens, one beep
 *   blue card on ENTRY again   -> DENIED "already inside", two low beeps
 *   red card on either reader  -> DENIED "unknown tag"
 *   blue card on EXIT reader   -> EXIT
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "core.h"
#include "config.h"
#include "rfid_door.h"

static const char *TAG = "lesson05";

static void drain_task(void *arg)
{
    event_t e;
    while (1) {
        if (xQueueReceive(core_event_queue(), &e, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "event %-7s uid=%08lX '%s'  -> %d inside",
                     core_event_name(e.type), (unsigned long)e.uid, e.note, core_people_inside());
        }
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(core_init());
    core_schedule_set(0, 24 * 60);     /* open all day; without an RTC the clock says 1970 */
    ESP_ERROR_CHECK(rfid_door_init());
    xTaskCreate(drain_task, "drain", 3072, NULL, 3, NULL);
}
