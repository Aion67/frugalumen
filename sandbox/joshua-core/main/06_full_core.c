/*
 * Lesson 06 — the real core module, exercised from app_main.
 *
 * Now we use components/core exactly as the other modules do. No hardware is
 * needed beyond the board (the RTC is optional; without it the clock starts at
 * 1970, which is why we widen the schedule to all day first).
 *
 * Watch for:
 *   - the second entry of the same tag being refused (anti-passback)
 *   - an exit for someone who never entered being refused
 *   - the event queue filling up: we drain it ourselves here because the
 *     storage module is not running in this sandbox
 */
#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "core.h"
#include "config.h"

static const char *TAG = "lesson06";

static void show_result(const char *what, esp_err_t err)
{
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "%-28s -> OK", what);
    } else if (err == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "%-28s -> REFUSED (anti-passback)", what);
    } else {
        ESP_LOGE(TAG, "%-28s -> error %s", what, esp_err_to_name(err));
    }
}

/* Pretend to be the storage task: pull events off the queue and print them. */
static void drain_task(void *arg)
{
    event_t e;
    while (1) {
        if (xQueueReceive(core_event_queue(), &e, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "  event: %-9s uid=%08lX note='%s'",
                     core_event_name(e.type), (unsigned long)e.uid, e.note);
        }
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(core_init());
    xTaskCreate(drain_task, "drain", 3072, NULL, 3, NULL);

    core_schedule_set(0, 24 * 60);   /* open all day so the clock value does not matter */
    time_t now = time(NULL);
    ESP_LOGI(TAG, "window open now? %s", core_access_window_open(now) ? "yes" : "no");

    uint32_t joan  = 0x01020304;     /* blue card in Wokwi */
    uint32_t peter = 0x11223344;     /* green card */
    char name[32];
    core_uid_known(joan, name, sizeof name);
    ESP_LOGI(TAG, "tag %08lX belongs to %s", (unsigned long)joan, name);

    show_result("Joan enters",            core_record_entry(joan, now));
    show_result("Joan enters AGAIN",      core_record_entry(joan, now));
    show_result("Peter exits (never in)", core_record_exit(peter, now));
    show_result("Peter enters",           core_record_entry(peter, now));
    ESP_LOGI(TAG, "people inside: %d", core_people_inside());
    show_result("Joan exits",             core_record_exit(joan, now));
    ESP_LOGI(TAG, "people inside: %d", core_people_inside());

    core_set_room_occupied(ROOM_2, true);
    ESP_LOGI(TAG, "room 2 occupied? %s", core_room_occupied(ROOM_2) ? "yes" : "no");

    uint16_t start = 0;
    uint16_t end   = 0;
    core_schedule_get(&start, &end);
    ESP_LOGI(TAG, "schedule %02u:%02u-%02u:%02u (saved in NVS, survives reset)",
             start / 60, start % 60, end / 60, end % 60);
}
