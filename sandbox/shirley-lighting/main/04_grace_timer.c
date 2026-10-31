/*
 * Lesson 04 — the grace period.
 *
 * Remember WHEN motion was last seen. Turn the light off only when that was
 * longer than LIGHT_GRACE_MS ago. We use the FreeRTOS tick counter as our
 * clock: xTaskGetTickCount() returns the number of ticks since boot, and
 * pdMS_TO_TICKS converts milliseconds to ticks so we can compare.
 *
 * Gotcha: the tick counter is unsigned and wraps after ~49 days. Subtracting
 * (now - last) still gives the right answer across the wrap, so always compute
 * a DIFFERENCE, never compare absolute values.
 */
#include <stdio.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "config.h"

static const char *TAG = "lesson04";

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

    bool       light_on    = false;
    TickType_t last_motion = 0;

    while (1) {
        bool       motion = gpio_get_level(PIN_PIR_ROOM1) == 1;
        TickType_t now    = xTaskGetTickCount();

        if (motion) {
            last_motion = now;                  /* restart the grace timer */
            if (!light_on) {
                gpio_set_level(PIN_LED_ROOM1, 1);
                light_on = true;
                ESP_LOGI(TAG, "light ON");
            }
        } else if (light_on) {
            TickType_t quiet_for = now - last_motion;
            if (quiet_for >= pdMS_TO_TICKS(LIGHT_GRACE_MS)) {
                gpio_set_level(PIN_LED_ROOM1, 0);
                light_on = false;
                ESP_LOGI(TAG, "light OFF after %d ms of no motion", LIGHT_GRACE_MS);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
