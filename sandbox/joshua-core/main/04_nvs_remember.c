/*
 * Lesson 04 — NVS: remembering a value across resets.
 *
 * RAM is wiped on reset. NVS (Non-Volatile Storage) is a small key-value store
 * in flash. We read a boot counter, add one, write it back. Press the reset
 * button in Wokwi and watch the number climb. core.c stores the schedule the
 * same way; auth.c stores the admin PIN.
 */
#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG = "lesson04";

void app_main(void)
{
    /* Step 1: initialise the NVS partition. The two special errors mean the
     * partition is from an older layout; erasing and retrying fixes it. */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    /* Step 2: open a "namespace" (like a folder) for read and write. */
    nvs_handle_t h;
    ESP_ERROR_CHECK(nvs_open("lesson", NVS_READWRITE, &h));

    /* Step 3: read. On the very first boot the key does not exist yet, and
     * nvs_get_u32 returns ESP_ERR_NVS_NOT_FOUND. That is normal, not a bug. */
    uint32_t boots = 0;
    err = nvs_get_u32(h, "boots", &boots);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "first boot ever");
    }

    boots = boots + 1;
    ESP_LOGI(TAG, "this is boot number %lu", (unsigned long)boots);

    /* Step 4: write and COMMIT. Without nvs_commit the value may not reach flash. */
    ESP_ERROR_CHECK(nvs_set_u32(h, "boots", boots));
    ESP_ERROR_CHECK(nvs_commit(h));
    nvs_close(h);

    ESP_LOGI(TAG, "press the reset button on the board to see it count up");
}
