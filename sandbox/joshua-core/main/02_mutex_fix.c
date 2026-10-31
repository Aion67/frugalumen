/*
 * Lesson 02 — the same race, fixed with a mutex.
 *
 * A mutex (MUTual EXclusion) is a lock only one task can hold at a time.
 * Take it, do the read-add-write, give it back. If the other task wants it
 * meanwhile, FreeRTOS puts that task to sleep until the lock is free.
 * Now the total is exactly 200000.
 *
 * This is the pattern core.c uses for every shared variable.
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"      /* mutexes live in the semaphore header */
#include "esp_log.h"

static const char *TAG = "lesson02";

#define INCREMENTS 100000

static volatile int      s_counter = 0;
static volatile int      s_done    = 0;
static SemaphoreHandle_t s_lock;   /* the mutex handle */

static void worker(void *arg)
{
    const char *name = arg;
    for (int i = 0; i < INCREMENTS; i++) {
        /* portMAX_DELAY = wait forever until we get the lock */
        xSemaphoreTake(s_lock, portMAX_DELAY);
        s_counter = s_counter + 1;         /* protected: nobody else can be here */
        xSemaphoreGive(s_lock);
    }
    ESP_LOGI(TAG, "%s finished", name);

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_done = s_done + 1;
    xSemaphoreGive(s_lock);
    vTaskDelete(NULL);
}

void app_main(void)
{
    s_lock = xSemaphoreCreateMutex();   /* must be created BEFORE any task uses it */

    xTaskCreatePinnedToCore(worker, "A", 2048, "task A", 5, NULL, 0);
    xTaskCreatePinnedToCore(worker, "B", 2048, "task B", 5, NULL, 1);

    while (s_done < 2) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    ESP_LOGI(TAG, "counter = %d, expected %d", s_counter, 2 * INCREMENTS);

    /* Gotcha: never hold the mutex while doing something slow (printing,
     * writing the SD card, vTaskDelay). Everyone else waiting on it stalls.
     * core.c therefore takes the lock, copies what it needs, gives it back,
     * and only THEN does slow work on the copy. */
}
