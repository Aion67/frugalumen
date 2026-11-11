/*
 * Lesson 05 — the real lighting component, two rooms, talking to core.
 *
 * lighting_init() does what lesson 04 did, but inside its own task and once
 * per room. Each room gets a separate context struct, so the two tasks never
 * share a variable. Every change goes to core (occupancy) and the event queue.
 *
 * We drain the queue here ourselves to see the events; in the full system the
 * storage task does that.
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "core.h"
#include "config.h"
#include "lighting.h"

static const char *TAG = "lesson05";

static void drain_task(void *arg)
{
    event_t e;
    while (1) {
        if (xQueueReceive(core_event_queue(), &e, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "event %s room %d", core_event_name(e.type), e.room + 1);
        }
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(core_init());
    ESP_ERROR_CHECK(lighting_init(ROOM_1, PIN_PIR_ROOM1, PIN_LED_ROOM1));
    ESP_ERROR_CHECK(lighting_init(ROOM_2, PIN_PIR_ROOM2, PIN_LED_ROOM2));
    xTaskCreate(drain_task, "drain", 3072, NULL, 3, NULL);

    while (1) {
        ESP_LOGI(TAG, "core says: room1 %s, room2 %s",
                 core_room_occupied(ROOM_1) ? "occupied" : "empty",
                 core_room_occupied(ROOM_2) ? "occupied" : "empty");
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}
