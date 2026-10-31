/*
 * door_servo.c — a hobby servo wants a pulse every 20 ms (50 Hz) whose width
 * sets the angle: ~0.5 ms = 0 degrees, ~2.5 ms = 180 degrees.
 * The LEDC peripheral produces that waveform in hardware; we only change the duty.
 */
#include "door_servo.h"
#include "config.h"
#include "esp_log.h"
#include "driver/ledc.h"

static const char *TAG = "door";

#define SERVO_TIMER        LEDC_TIMER_0
#define SERVO_MODE         LEDC_LOW_SPEED_MODE
#define SERVO_RESOLUTION   LEDC_TIMER_14_BIT
#define SERVO_FREQ_HZ      50
#define SERVO_PERIOD_US    20000
#define SERVO_CLOSED_US    600     /* ~0 degrees   */
#define SERVO_OPEN_US      1500    /* ~90 degrees  */

static const ledc_channel_t s_channel[2] = { LEDC_CHANNEL_0, LEDC_CHANNEL_1 };
static const int            s_pin[2]     = { PIN_SERVO_ENTRY, PIN_SERVO_EXIT };

static uint32_t pulse_to_duty(uint32_t pulse_us)
{
    uint32_t max_duty = (1 << 14) - 1;
    return (pulse_us * max_duty) / SERVO_PERIOD_US;
}

esp_err_t door_servo_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode      = SERVO_MODE,
        .duty_resolution = SERVO_RESOLUTION,
        .timer_num       = SERVO_TIMER,
        .freq_hz         = SERVO_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    esp_err_t err = ledc_timer_config(&timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "servo timer config failed");
        return err;
    }

    for (int lane = 0; lane < 2; lane++) {
        ledc_channel_config_t ch = {
            .speed_mode = SERVO_MODE,
            .channel    = s_channel[lane],
            .timer_sel  = SERVO_TIMER,
            .intr_type  = LEDC_INTR_DISABLE,
            .gpio_num   = s_pin[lane],
            .duty       = pulse_to_duty(SERVO_CLOSED_US),
            .hpoint     = 0,
        };
        err = ledc_channel_config(&ch);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "servo channel %d config failed", lane);
            return err;
        }
    }
    return ESP_OK;
}

void door_servo_set(lane_t lane, bool open)
{
    uint32_t pulse = open ? SERVO_OPEN_US : SERVO_CLOSED_US;
    ledc_set_duty(SERVO_MODE, s_channel[lane], pulse_to_duty(pulse));
    ledc_update_duty(SERVO_MODE, s_channel[lane]);
}
