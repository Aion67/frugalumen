/*
 * Lesson 02 — append lines to a file and read them back.
 *
 * Standard C file functions work on the mounted card. Two habits matter on an
 * embedded device:
 *   - open in "a" (append) mode so old lines are kept
 *   - fclose() after every write; if power drops, nothing is left in a buffer
 * Press reset a few times: the file keeps growing. That is "survives reset".
 */
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/spi_master.h"
#include "config.h"

static const char *TAG = "lesson02";

#define FILE_PATH SD_MOUNT_POINT "/lesson02.txt"

static esp_err_t mount(void)
{
    spi_bus_config_t bus = {
        .sclk_io_num = PIN_SPI_SCK, .mosi_io_num = PIN_SPI_MOSI, .miso_io_num = PIN_SPI_MISO,
        .quadwp_io_num = -1, .quadhd_io_num = -1, .max_transfer_sz = 4096,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI2_HOST;
    host.max_freq_khz = 5000;
    sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot.host_id = SPI2_HOST;
    slot.gpio_cs = PIN_SD_CS;
    esp_vfs_fat_sdmmc_mount_config_t cfg = { .format_if_mount_failed = true, .max_files = 2,
                                             .allocation_unit_size = 16 * 1024 };
    sdmmc_card_t *card;
    return esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &host, &slot, &cfg, &card);
}

void app_main(void)
{
    if (mount() != ESP_OK) {
        ESP_LOGE(TAG, "no card");
        return;
    }

    /* append one line */
    FILE *f = fopen(FILE_PATH, "a");
    if (f == NULL) {
        ESP_LOGE(TAG, "fopen for append failed");
        return;
    }
    fprintf(f, "boot at tick %lu\n", (unsigned long)xTaskGetTickCount());
    fclose(f);
    ESP_LOGI(TAG, "appended a line");

    /* read everything back, line by line */
    f = fopen(FILE_PATH, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "fopen for read failed");
        return;
    }
    char line[64];
    int count = 0;
    while (fgets(line, sizeof line, f) != NULL) {     /* fgets keeps the '\n' */
        count++;
        line[strcspn(line, "\n")] = '\0';             /* strip it for a tidy log */
        ESP_LOGI(TAG, "line %d: %s", count, line);
    }
    fclose(f);
    ESP_LOGI(TAG, "%d lines so far - press reset and watch it grow", count);
}
