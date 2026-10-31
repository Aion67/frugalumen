/*
 * lighting.c — automatic lights for one room.
 *
 * Motion on the PIR turns the light on at once. The light goes off only after
 * LIGHT_GRACE_MS without motion, so someone sitting still does not get plunged
 * into darkness. Every change is reported to core (occupancy) and logged.
 *
 * There is one task per room; each task gets its own room_ctx_t so the two
 * rooms never share a variable.
 */
#include "lighting.h"
#include "config.h"
#include "esp_log.h"
#include "freertos/task.h"

static const char *TAG = "lighting";

typedef struct {
    room_id_t  room;
    gpio_num_t pir_pin;
    gpio_num_t led_pin;
    bool       light_on;
    TickType_t last_motion_tick;
} room_ctx_t;

static room_ctx_t s_rooms[ROOM_COUNT];

static void lighting_task(void *arg);

static void report(room_ctx_t *ctx, bool on)
{
    event_t e = { 0 };
    e.ts   = time(NULL);
    e.type = on ? EV_LIGHT_ON : EV_LIGHT_OFF;
    e.uid  = 0;
    e.room = ctx->room;
    snprintf(e.note, sizeof e.note, "room %d %s", ctx->room + 1, on ? "on" : "off");
    core_set_room_occupied(ctx->room, on);
    core_emit(&e);
    ESP_LOGI(TAG, "room %d light %s", ctx->room + 1, on ? "ON" : "OFF");
}

esp_err_t lighting_init(room_id_t room, gpio_num_t pir_pin, gpio_num_t led_pin)
{
    if (room >= ROOM_COUNT) {
        ESP_LOGE(TAG, "bad room id %d", room);
        return ESP_ERR_INVALID_ARG;
    }

    gpio_config_t pir = {
        .pin_bit_mask = 1ULL << pir_pin,
        .mode         = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,   /* idle LOW, PIR drives HIGH on motion */
        .intr_type    = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&pir);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to configure PIR pin %d", pir_pin);
        return err;
    }

    gpio_config_t led = {
        .pin_bit_mask = 1ULL << led_pin,
        .mode         = GPIO_MODE_OUTPUT,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    err = gpio_config(&led);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to configure LED pin %d", led_pin);
        return err;
    }
    gpio_set_level(led_pin, 0);

    room_ctx_t *ctx = &s_rooms[room];
    ctx->room     = room;
    ctx->pir_pin  = pir_pin;
    ctx->led_pin  = led_pin;
    ctx->light_on = false;

    char name[16];
    snprintf(name, sizeof name, "light%d", room + 1);
    BaseType_t ok = xTaskCreate(lighting_task, name, 2560, ctx, 5, NULL);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "could not start task for room %d", room + 1);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "room %d ready (PIR %d, LED %d)", room + 1, pir_pin, led_pin);
    return ESP_OK;
}

static void lighting_task(void *arg)
{
    room_ctx_t *ctx = arg;

    while (1) {
        bool motion = gpio_get_level(ctx->pir_pin) == 1;
        TickType_t now = xTaskGetTickCount();

        if (motion) {
            ctx->last_motion_tick = now;
            if (!ctx->light_on) {
                gpio_set_level(ctx->led_pin, 1);
                ctx->light_on = true;
                report(ctx, true);
            }
        } else if (ctx->light_on) {
            TickType_t quiet_for = now - ctx->last_motion_tick;
            if (quiet_for >= pdMS_TO_TICKS(LIGHT_GRACE_MS)) {
                gpio_set_level(ctx->led_pin, 0);
                ctx->light_on = false;
                report(ctx, false);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(PIR_POLL_MS));
    }
}
