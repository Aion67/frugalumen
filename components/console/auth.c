/*
 * auth.c — one admin PIN, stored in NVS under namespace "cocis", key "pin".
 * On first boot there is no saved PIN, so ADMIN_PIN_DEFAULT from config.h is used.
 */
#include <string.h>
#include "auth.h"
#include "config.h"
#include "esp_log.h"
#include "nvs.h"

static const char *TAG = "auth";

static char s_pin[ADMIN_PIN_MAX_LEN + 1] = ADMIN_PIN_DEFAULT;

esp_err_t auth_init(void)
{
    nvs_handle_t h;
    if (nvs_open("cocis", NVS_READONLY, &h) != ESP_OK) {
        ESP_LOGI(TAG, "using default PIN");
        return ESP_OK;
    }
    size_t len = sizeof s_pin;
    if (nvs_get_str(h, "pin", s_pin, &len) == ESP_OK) {
        ESP_LOGI(TAG, "PIN loaded from NVS");
    }
    nvs_close(h);
    return ESP_OK;
}

bool auth_check_pin(const char *pin)
{
    return strcmp(pin, s_pin) == 0;
}

esp_err_t auth_set_pin(const char *new_pin)
{
    size_t len = strlen(new_pin);
    if (len < 4 || len > ADMIN_PIN_MAX_LEN) {
        return ESP_ERR_INVALID_ARG;
    }
    for (size_t i = 0; i < len; i++) {
        if (new_pin[i] < '0' || new_pin[i] > '9') {
            return ESP_ERR_INVALID_ARG;
        }
    }

    nvs_handle_t h;
    esp_err_t err = nvs_open("cocis", NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed");
        return err;
    }
    nvs_set_str(h, "pin", new_pin);
    err = nvs_commit(h);
    nvs_close(h);
    if (err != ESP_OK) {
        return err;
    }
    strlcpy(s_pin, new_pin, sizeof s_pin);
    return ESP_OK;
}
