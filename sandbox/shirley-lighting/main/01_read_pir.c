/*
 * Lesson 01 — just read the PIR sensor and print.
 *
 * A PIR (passive infrared) sensor has one output pin. It is LOW when nothing
 * moves and goes HIGH for a few seconds when it sees motion. In Wokwi, click
 * the sensor while the simulation runs and press "Simulate Motion".
 */
#include <stdio.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "config.h"            /* PIN_PIR_ROOM1 lives here, never in this file */

static const char *TAG = "lesson01";

void app_main(void)
{
    /* gpio_config_t describes one or more pins. pin_bit_mask is a 64-bit
     * number with one bit set per pin, hence the 1ULL << pin. */
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << PIN_PIR_ROOM1,
        .mode         = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,   /* keep the pin LOW when the sensor is quiet */
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));

    while (1) {
        /* gpio_get_level() reads the pin RIGHT NOW: 1 = HIGH = motion. */
        int level = gpio_get_level(PIN_PIR_ROOM1);
        ESP_LOGI(TAG, "PIR = %d %s", level, level ? "(motion!)" : "");

        /* vTaskDelay sleeps THIS task and lets others run. A loop with no
         * delay would starve the idle task and trip the watchdog. */
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
