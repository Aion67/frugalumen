/*
 * buzzer.c — a passive buzzer needs a square wave to make sound.
 * LEDC gives us that: set a frequency, 50% duty = tone, 0% duty = silence.
 */
#include "buzzer.h"
#include "config.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "buzzer";

#define BUZZER_TIMER       LEDC_TIMER_1
#define BUZZER_MODE        LEDC_LOW_SPEED_MODE
#define BUZZER_CHANNEL     LEDC_CHANNEL_2
#define BUZZER_RESOLUTION  LEDC_TIMER_10_BIT
#define BUZZER_HALF_DUTY   512

static void tone(uint32_t freq_hz, int duration_ms)
{
    ledc_set_freq(BUZZER_MODE, BUZZER_TIMER, freq_hz);
    ledc_set_duty(BUZZER_MODE, BUZZER_CHANNEL, BUZZER_HALF_DUTY);
    ledc_update_duty(BUZZER_MODE, BUZZER_CHANNEL);
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    ledc_set_duty(BUZZER_MODE, BUZZER_CHANNEL, 0);
    ledc_update_duty(BUZZER_MODE, BUZZER_CHANNEL);
}

esp_err_t buzzer_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode      = BUZZER_MODE,
        .duty_resolution = BUZZER_RESOLUTION,
        .timer_num       = BUZZER_TIMER,
        .freq_hz         = 2000,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    esp_err_t err = ledc_timer_config(&timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "timer config failed");
        return err;
    }
    ledc_channel_config_t ch = {
        .speed_mode = BUZZER_MODE,
        .channel    = BUZZER_CHANNEL,
        .timer_sel  = BUZZER_TIMER,
        .intr_type  = LEDC_INTR_DISABLE,
        .gpio_num   = PIN_BUZZER,
        .duty       = 0,
        .hpoint     = 0,
    };
    err = ledc_channel_config(&ch);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "channel config failed");
        return err;
    }
    return ESP_OK;
}

void buzzer_beep_ok(void)
{
    tone(2000, 120);
}

void buzzer_beep_deny(void)
{
    tone(400, 150);
    vTaskDelay(pdMS_TO_TICKS(80));
    tone(400, 150);
}
