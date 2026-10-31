#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"

#include "storage.h"
#include "config.h"      // pin names (PIN_SPI_SCK, PIN_SD_CS ...)

static const char *TAG = "storage";

#define SD_SPI_HOST     SPI2_HOST           // SPI bus used by the SD card
#define MOUNT_POINT     "/sdcard"           // the card appears as this folder
#define LOG_FILE        MOUNT_POINT "/events.csv"
#define LINE_SIZE       100                 // longest CSV line we expect is about 65 characters
#define LOGGER_STACK    4096
#define LOGGER_PRIORITY 5

static sdmmc_card_t *card;                  // info about the card once mounted
static SemaphoreHandle_t file_mutex = NULL; // only one task may touch the file at a time

/* ---------- 1. Mounting the SD card ---------- */

// Starts the SPI bus and mounts the SD card
static esp_err_t sd_mount(void)
{
    // SPI bus pins (shared with the RFID reader, names come from config.h)
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_SPI_MOSI,
        .miso_io_num = PIN_SPI_MISO,
        .sclk_io_num = PIN_SPI_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    esp_err_t ret = spi_bus_initialize(SD_SPI_HOST, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret == ESP_ERR_INVALID_STATE) {
        // the RFID module already started the bus, that is fine
        ESP_LOGW(TAG, "SPI bus already initialised, continuing");
    } else if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Mount settings
    esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {
        .format_if_mount_failed = true,     // the simulated card may start empty
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
    };

    // SD card on the SPI bus with its own chip select pin
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SD_SPI_HOST;

    sdspi_device_config_t slot_cfg = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_cfg.gpio_cs = PIN_SD_CS;
    slot_cfg.host_id = SD_SPI_HOST;

    // Mount it. We check the result ourselves so a missing card does not crash the system
    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_cfg, &mount_cfg, &card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SD mount failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "SD card mounted");
    return ESP_OK;
}

/* ---------- 2. Writing one event as one CSV line ---------- */

// Adds one event to the end of the log file
static void write_event_line(const event_t *e)
{
    // copy the note and replace commas and new lines, because they would break the CSV columns
    char note[24];
    strncpy(note, e->note, sizeof(note) - 1);
    note[sizeof(note) - 1] = '\0';
    for (int i = 0; note[i] != '\0'; i++) {
        if (note[i] == ',' || note[i] == '\n' || note[i] == '\r') {
            note[i] = ' ';
        }
    }

    FILE *f = fopen(LOG_FILE, "a");         // "a" means append to the end
    if (f == NULL) {
        ESP_LOGE(TAG, "Could not open %s", LOG_FILE);
        return;
    }
    // format: timestamp,type,uid,room,note
    fprintf(f, "%lld,%d,%" PRIu32 ",%d,%s\n",
            (long long)e->ts, (int)e->type, e->uid, (int)e->room, note);
    fclose(f);                              // closing saves the data to the card
}

// The logger task: waits for events in the queue and saves each one
static void logger_task(void *arg)
{
    (void)arg;
    event_t e;
    while (1) {
        // sleep until an event arrives
        if (xQueueReceive(core_event_queue(), &e, portMAX_DELAY) == pdTRUE) {
            xSemaphoreTake(file_mutex, portMAX_DELAY);   // lock the file
            write_event_line(&e);
            xSemaphoreGive(file_mutex);                  // unlock the file
        }
    }
}

/* ---------- 3. Reading lines back ---------- */

// Turns one CSV line into an event. Returns true if the line was valid
static bool parse_line(const char *line, event_t *e)
{
    long long ts;
    int type, room;
    uint32_t uid;
    char note[24] = "";

    int n = sscanf(line, "%lld,%d,%" SCNu32 ",%d,%23[^\r\n]",
                   &ts, &type, &uid, &room, note);
    if (n < 4) {                            // the note is optional, the first 4 fields are not
        return false;
    }

    e->ts = (time_t)ts;
    e->type = (event_type_t)type;
    e->uid = uid;
    e->room = (room_id_t)room;
    strncpy(e->note, note, sizeof(e->note) - 1);
    e->note[sizeof(e->note) - 1] = '\0';
    return true;
}

/* ---------- Public functions (the contract) ---------- */

esp_err_t storage_init(void)
{
    if (file_mutex != NULL) {
        ESP_LOGE(TAG, "Storage is already initialised");
        return ESP_ERR_INVALID_STATE;
    }

    // the event queue belongs to core, so core_init() must run first
    if (core_event_queue() == NULL) {
        ESP_LOGE(TAG, "Event queue not ready, call core_init() first");
        return ESP_ERR_INVALID_STATE;
    }

    file_mutex = xSemaphoreCreateMutex();
    if (file_mutex == NULL) {
        ESP_LOGE(TAG, "Could not create file mutex");
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = sd_mount();
    if (ret != ESP_OK) {
        vSemaphoreDelete(file_mutex);
        file_mutex = NULL;
        return ret;
    }

    // start the logger task that saves events
    BaseType_t ok = xTaskCreate(logger_task, "logger", LOGGER_STACK, NULL, LOGGER_PRIORITY, NULL);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "Could not start logger task");
        esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card);
        card = NULL;
        vSemaphoreDelete(file_mutex);
        file_mutex = NULL;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Storage ready");
    return ESP_OK;
}

// Reads the last n events. Returns how many were read
int storage_read_last(int n, event_t *out)
{
    if (n <= 0 || out == NULL || file_mutex == NULL) {
        return 0;
    }

    xSemaphoreTake(file_mutex, portMAX_DELAY);

    FILE *f = fopen(LOG_FILE, "r");
    if (f == NULL) {                        // no file yet means no events yet
        xSemaphoreGive(file_mutex);
        return 0;
    }

    char line[LINE_SIZE];

    // first pass: count the lines in the file
    int total = 0;
    while (fgets(line, sizeof(line), f) != NULL) {
        total++;
    }

    // second pass: skip the older lines and keep the last n
    int skip = total - n;
    if (skip < 0) {
        skip = 0;
    }
    rewind(f);

    int index = 0;
    int count = 0;
    while (fgets(line, sizeof(line), f) != NULL && count < n) {
        if (index >= skip) {
            if (parse_line(line, &out[count])) {
                count++;
            }
        }
        index++;
    }

    fclose(f);
    xSemaphoreGive(file_mutex);
    return count;
}

// Counts how many logged events have this type
int storage_count(event_type_t type)
{
    if (file_mutex == NULL) {
        return 0;
    }

    xSemaphoreTake(file_mutex, portMAX_DELAY);

    FILE *f = fopen(LOG_FILE, "r");
    if (f == NULL) {
        xSemaphoreGive(file_mutex);
        return 0;
    }

    char line[LINE_SIZE];
    event_t e;
    int count = 0;
    while (fgets(line, sizeof(line), f) != NULL) {
        if (parse_line(line, &e) && e.type == type) {
            count++;
        }
    }

    fclose(f);
    xSemaphoreGive(file_mutex);
    return count;
}