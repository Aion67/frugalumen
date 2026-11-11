/*
 * display.c — show the building state on the LCD.
 *
 *   line 1:  In: 3 L1:* L2:-        people inside, light per room (* on, - off)
 *   line 2:  09:41 OPEN             clock and access-window state
 *            18:05 OVERSTAY 2       window closed but people still inside
 *
 * The task only reads core; it never changes state. It rewrites the LCD only
 * when the text actually changed, which keeps the I2C bus quiet.
 */
#include <string.h>
#include "display.h"
#include "config.h"
#include "lcd1602.h"
#include "esp_log.h"
#include "freertos/task.h"

static const char *TAG = "display";

static void display_task(void *arg);

static char light_symbol(room_id_t room)
{
    if (core_room_occupied(room)) {
        return '*';
    }
    return '-';
}

static void build_lines(char *line1, char *line2, size_t len)
{
    int inside = core_people_inside();
    snprintf(line1, len, "In:%2d L1:%c L2:%c", inside, light_symbol(ROOM_1), light_symbol(ROOM_2));

    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);

    if (core_access_window_open(now)) {
        snprintf(line2, len, "%02d:%02d OPEN", t.tm_hour, t.tm_min);
    } else if (inside > 0) {
        snprintf(line2, len, "%02d:%02d OVERSTAY %d", t.tm_hour, t.tm_min, inside);
    } else {
        snprintf(line2, len, "%02d:%02d CLOSED", t.tm_hour, t.tm_min);
    }
}

esp_err_t display_init(void)
{
    esp_err_t err = lcd1602_init(core_i2c_bus());
    if (err != ESP_OK) {
        return err;
    }
    lcd1602_write_line(0, "CoCIS SCM");
    lcd1602_write_line(1, "starting...");

    BaseType_t ok = xTaskCreate(display_task, "display", 3072, NULL, 2, NULL);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "could not start task");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "LCD ready");
    return ESP_OK;
}

static void display_task(void *arg)
{
    char shown1[20] = "";
    char shown2[20] = "";
    char line1[20];
    char line2[20];

    while (1) {
        build_lines(line1, line2, sizeof line1);

        if (strcmp(line1, shown1) != 0) {
            lcd1602_write_line(0, line1);
            strcpy(shown1, line1);
        }
        if (strcmp(line2, shown2) != 0) {
            lcd1602_write_line(1, line2);
            strcpy(shown2, line2);
        }
        vTaskDelay(pdMS_TO_TICKS(DISPLAY_REFRESH_MS));
    }
}
