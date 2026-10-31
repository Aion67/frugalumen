/*
 * Lesson 03 — light follows motion, no delay yet.
 *
 * Put lessons 01 and 02 together: LED = PIR. Notice the problem: the PIR
 * output drops a few seconds after each movement, so the light flickers off
 * whenever someone sits still. Lesson 04 fixes that.
 */
#include <stdio.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "config.h"

static const char *TAG = "lesson03";

void app_main(void)
{
    gpio_config_t pir = {
        .pin_bit_mask = 1ULL << PIN_PIR_ROOM1,
        .mode         = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
    };
    gpio_config_t led = {
        .pin_bit_mask = 1ULL << PIN_LED_ROOM1,
        .mode         = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&pir));
    ESP_ERROR_CHECK(gpio_config(&led));

    int last = -1;
    while (1) {
        int motion = gpio_get_level(PIN_PIR_ROOM1);
        gpio_set_level(PIN_LED_ROOM1, motion);
        if (motion != last) {                 /* only print on change, not every loop */
            ESP_LOGI(TAG, "light %s", motion ? "ON" : "OFF");
            last = motion;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
