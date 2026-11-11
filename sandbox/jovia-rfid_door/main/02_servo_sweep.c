/*
 * Lesson 02 — move the door servo with LEDC PWM.
 *
 * A servo listens for a pulse every 20 ms (50 Hz). Pulse width = angle:
 * about 0.5 ms = 0 degrees, 1.5 ms = 90, 2.5 ms = 180. LEDC is the ESP32's
 * PWM peripheral: a TIMER sets the frequency, a CHANNEL ties that timer to a
 * pin and holds the duty (how long the pulse is high per period).
 *
 * duty is a fraction of 2^resolution. At 14-bit resolution the period is
 * 16383 steps, so a 1.5 ms pulse = 1.5/20 * 16383 = 1228.
 */
#include <stdio.h>
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "config.h"

static const char *TAG = "lesson02";

static uint32_t pulse_us_to_duty(uint32_t pulse_us)
{
    uint32_t max_duty = (1 << 14) - 1;        /* 14-bit timer */
    return pulse_us * max_duty / 20000;       /* 20000 us period */
}

void app_main(void)
{
    ledc_timer_config_t timer = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_14_BIT,
        .timer_num       = LEDC_TIMER_0,
        .freq_hz         = 50,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    ledc_channel_config_t channel = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_0,
        .timer_sel  = LEDC_TIMER_0,
        .gpio_num   = PIN_SERVO_ENTRY,
        .duty       = pulse_us_to_duty(600),  /* start closed */
        .hpoint     = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel));

    while (1) {
        ESP_LOGI(TAG, "door OPEN (90 degrees)");
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, pulse_us_to_duty(1500));
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);   /* set_duty alone does nothing until update */
        vTaskDelay(pdMS_TO_TICKS(DOOR_OPEN_MS));

        ESP_LOGI(TAG, "door CLOSED (0 degrees)");
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, pulse_us_to_duty(600));
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
