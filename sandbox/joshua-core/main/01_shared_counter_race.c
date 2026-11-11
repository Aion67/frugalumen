/*
 * Lesson 01 — what goes wrong WITHOUT a mutex.
 *
 * Two tasks both do "counter = counter + 1" many times. In C that single line
 * is really three steps: read counter, add one, write it back. If task A reads,
 * then task B reads before A has written, both write the same value and one
 * increment is lost. The final total is then LESS than expected.
 *
 * Expected: 2 tasks x 100000 = 200000. Watch the printed total fall short.
 * (On a dual-core ESP32 the loss is large; on one core it may be small, so we
 * pin the tasks to different cores to make the race obvious.)
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "lesson01";

#define INCREMENTS 100000

/* "volatile" tells the compiler the value may change behind its back, so it
 * must really read memory each time instead of keeping a copy in a register. */
static volatile int s_counter = 0;
static volatile int s_done    = 0;

static void worker(void *arg)
{
    const char *name = arg;
    for (int i = 0; i < INCREMENTS; i++) {
        s_counter = s_counter + 1;     /* NOT atomic: read, add, write */
    }
    ESP_LOGI(TAG, "%s finished", name);
    s_done = s_done + 1;               /* (also racy, but only two writers once each) */
    vTaskDelete(NULL);                 /* a task must delete itself, it must not just return */
}

void app_main(void)
{
    /* xTaskCreatePinnedToCore(function, name, stack bytes, argument, priority, handle, core) */
    xTaskCreatePinnedToCore(worker, "A", 2048, "task A", 5, NULL, 0);
    xTaskCreatePinnedToCore(worker, "B", 2048, "task B", 5, NULL, 1);

    while (s_done < 2) {
        vTaskDelay(pdMS_TO_TICKS(100));   /* sleep, let the workers run */
    }
    ESP_LOGI(TAG, "counter = %d, expected %d, lost %d",
             s_counter, 2 * INCREMENTS, 2 * INCREMENTS - s_counter);
}
