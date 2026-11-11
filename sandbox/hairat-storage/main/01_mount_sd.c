/*
 * Lesson 01 — mount the microSD card over SPI.
 *
 * The card speaks SPI like the RFID readers, so it shares SCK/MOSI/MISO and
 * gets its own CS (pin 15). ESP-IDF's "sdspi" host drives it and FATFS makes
 * it look like files: after mounting, fopen("/sdcard/x.txt") just works.
 *
 * Wokwi's card starts formatted (FAT16, up to 8 MB), so format_if_mount_failed
 * is only a safety net.
 */
#include <stdio.h>
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/spi_master.h"
#include "config.h"

static const char *TAG = "lesson01";

void app_main(void)
{
    /* Step 1: the SPI bus (in the full system core does this once for everyone). */
    spi_bus_config_t bus = {
        .sclk_io_num = PIN_SPI_SCK, .mosi_io_num = PIN_SPI_MOSI, .miso_io_num = PIN_SPI_MISO,
        .quadwp_io_num = -1, .quadhd_io_num = -1, .max_transfer_sz = 4096,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));

    /* Step 2: host = "talk SD protocol over SPI on this bus, at this speed". */
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot         = SPI2_HOST;
    host.max_freq_khz = 5000;

    /* Step 3: slot = which CS pin this card hangs on. */
    sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot.host_id = SPI2_HOST;
    slot.gpio_cs = PIN_SD_CS;

    /* Step 4: mount it as a filesystem at /sdcard. */
    esp_vfs_fat_sdmmc_mount_config_t mount = {
        .format_if_mount_failed = true,
        .max_files              = 2,
        .allocation_unit_size   = 16 * 1024,
    };
    sdmmc_card_t *card;
    esp_err_t err = esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &host, &slot, &mount, &card);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mount failed: %s. Is the card wired to CS 15?", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "mounted! card info follows:");
    sdmmc_card_print_info(stdout, card);   /* name, size, speed */
}
