/*
 * Lesson 03 — the real display, fed by changing core state.
 *
 * display_init() starts a task that reads core once a second and rewrites the
 * LCD only when the text changed. We fake some traffic so you can watch
 * line 1 (people inside, lights) and line 2 (clock, OPEN/CLOSED/OVERSTAY) move.
 * Needs the LCD on 21/22. The RTC is optional (without it the clock is 1970).
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "core.h"
#include "config.h"
#include "display.h"

static const char *TAG = "lesson03";

void app_main(void)
{
    ESP_ERROR_CHECK(core_init());
    ESP_ERROR_CHECK(display_init());

    /* Keep the queue from filling: nobody consumes events in this sandbox. */
    xQueueReset(core_event_queue());

    core_schedule_set(0, 24 * 60);
    time_t now = time(NULL);

    vTaskDelay(pdMS_TO_TICKS(3000));
    ESP_LOGI(TAG, "two people enter");
    core_record_entry(0x01020304, now);
    core_record_entry(0x11223344, now);

    vTaskDelay(pdMS_TO_TICKS(3000));
    ESP_LOGI(TAG, "room 1 light on");
    core_set_room_occupied(ROOM_1, true);

    vTaskDelay(pdMS_TO_TICKS(3000));
    ESP_LOGI(TAG, "one person leaves");
    core_record_exit(0x01020304, now);

    vTaskDelay(pdMS_TO_TICKS(3000));
    ESP_LOGI(TAG, "close the access window with someone still inside -> OVERSTAY");
    core_schedule_set(1, 2);   /* 00:01-00:02: closed at any other time */
}
