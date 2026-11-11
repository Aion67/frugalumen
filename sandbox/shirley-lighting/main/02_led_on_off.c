/*
 * Lesson 02 — drive the LED.
 *
 * An output pin is either 0 V (LOW) or 3.3 V (HIGH). With the LED's anode on
 * the pin (through a resistor) and cathode to GND, HIGH = on.
 */
#include <stdio.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "config.h"

static const char *TAG = "lesson02";

void app_main(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << PIN_LED_ROOM1,
        .mode         = GPIO_MODE_OUTPUT,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));

    bool on = false;
    while (1) {
        on = !on;
        gpio_set_level(PIN_LED_ROOM1, on ? 1 : 0);
        ESP_LOGI(TAG, "LED %s", on ? "ON" : "OFF");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
