/*
 * Lesson 03 — beep the buzzer.
 *
 * A passive buzzer makes no sound on a steady voltage; it needs a square wave.
 * LEDC again: the frequency is the pitch, 50% duty = sound, 0% = silence.
 * We use a different LEDC timer than the servo because the servo timer is
 * locked to 50 Hz.
 */
#include <stdio.h>
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "config.h"

static const char *TAG = "lesson03";

static void tone(uint32_t freq_hz, int ms)
{
    ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_1, freq_hz);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, 512);   /* 50% of 10-bit (1024) */
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2);
    vTaskDelay(pdMS_TO_TICKS(ms));
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, 0);     /* silence */
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2);
}

void app_main(void)
{
    ledc_timer_config_t timer = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num       = LEDC_TIMER_1,
        .freq_hz         = 2000,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));
    ledc_channel_config_t channel = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_2,
        .timer_sel  = LEDC_TIMER_1,
        .gpio_num   = PIN_BUZZER,
        .duty       = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel));

    while (1) {
        ESP_LOGI(TAG, "OK beep: one short high tone");
        tone(2000, 120);
        vTaskDelay(pdMS_TO_TICKS(1500));

        ESP_LOGI(TAG, "DENY beep: two low tones");
        tone(400, 150);
        vTaskDelay(pdMS_TO_TICKS(80));
        tone(400, 150);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}
