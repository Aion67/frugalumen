/*
 * Lesson 03 — a queue: many producers, one consumer.
 *
 * Problem: the SD card is slow and only one task should write to it. But
 * events come from the door task, two lighting tasks and the console.
 * Answer: a FreeRTOS queue. Producers drop a copy of the event in and move on
 * immediately. One consumer task blocks on the queue and writes in order.
 * Nobody waits for the SD card except the logger itself.
 *
 * This file is the whole idea of core_emit() + the storage task in miniature.
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

static const char *TAG = "lesson03";

typedef struct {
    int  producer_id;
    int  sequence;
    char note[16];
} message_t;

static QueueHandle_t s_queue;

static void producer(void *arg)
{
    int id = (int)arg;
    for (int i = 0; i < 5; i++) {
        message_t m = { .producer_id = id, .sequence = i };
        snprintf(m.note, sizeof m.note, "hello %d", i);

        /* xQueueSend COPIES the struct into the queue, so m can go out of
         * scope safely. Timeout 0 = if the queue is full, give up at once
         * rather than block. core_emit() does exactly this. */
        if (xQueueSend(s_queue, &m, 0) != pdTRUE) {
            ESP_LOGW(TAG, "producer %d: queue full, message dropped", id);
        }
        vTaskDelay(pdMS_TO_TICKS(100 * id));   /* producers run at different speeds */
    }
    vTaskDelete(NULL);
}

static void consumer(void *arg)
{
    message_t m;
    while (1) {
        /* Blocks (sleeps) until a message arrives. No polling, no CPU waste. */
        if (xQueueReceive(s_queue, &m, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "consumer got: producer %d seq %d '%s'", m.producer_id, m.sequence, m.note);
            vTaskDelay(pdMS_TO_TICKS(250));    /* pretend the SD card is slow */
        }
    }
}

void app_main(void)
{
    /* 8 slots, each the size of one message_t */
    s_queue = xQueueCreate(8, sizeof(message_t));

    xTaskCreate(consumer, "consumer", 2048, NULL, 4, NULL);
    xTaskCreate(producer, "p1", 2048, (void *)1, 5, NULL);
    xTaskCreate(producer, "p2", 2048, (void *)2, 5, NULL);
    xTaskCreate(producer, "p3", 2048, (void *)3, 5, NULL);
    /* app_main may return; the tasks keep running. */
}
