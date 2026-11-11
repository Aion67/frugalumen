/*
 * Lesson 04 — the real console on top of core and storage.
 *
 * console_init() installs the UART driver and starts the menu task. Nothing
 * else is needed. We add a little fake traffic so the menu has something to
 * show: two entries, one exit, a light.
 *
 * Try: 1 (occupancy), 2 (who), 4 (log), 7 (report), 5 with PIN 2468, then
 * reset the board and check 8: the schedule you set is still there (NVS).
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "core.h"
#include "config.h"
#include "storage.h"
#include "console.h"

static void fake_traffic(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(1500));
    time_t now = time(NULL);
    core_record_entry(0x01020304, now);
    core_record_entry(0x11223344, now);
    core_record_exit(0x01020304, now);
    core_set_room_occupied(ROOM_1, true);
    event_t e = { .ts = now, .type = EV_LIGHT_ON, .room = ROOM_1, .note = "room 1 on" };
    core_emit(&e);
    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_ERROR_CHECK(core_init());
    ESP_ERROR_CHECK(storage_init());     /* RAM log is fine; no card needed here */
    ESP_ERROR_CHECK(console_init());
    xTaskCreate(fake_traffic, "fake", 3072, NULL, 3, NULL);
}
