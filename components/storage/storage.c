/*
 * storage.c — event logger.
 *
 * One task blocks on the core event queue. Each event is appended as one CSV
 * line to /sdcard/events.csv and also kept in a small RAM ring buffer. The
 * ring buffer means the system still works (and the console still shows
 * recent events) when the card is missing.
 *
 * The file is a shared resource between this task (writer) and the console
 * task (reader), so every file access is wrapped in s_file_lock.
 *
 * CSV line format:  <unix time>,<TYPE>,<uid hex>,<room or ->,<note>
 */
#include <stdio.h>
#include <string.h>
#include "storage.h"
#include "config.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "storage";

static SemaphoreHandle_t s_file_lock;
static sdmmc_card_t     *s_card;
static bool              s_mounted;

/* RAM ring buffer: s_ram[s_ram_head] is the next slot to write */
static event_t s_ram[RAM_LOG_LEN];
static int     s_ram_head;
static int     s_ram_count;

static void storage_task(void *arg);

/* ------------------------------------------------------------------ */
/* SD card                                                             */
/* ------------------------------------------------------------------ */

static esp_err_t sd_mount(void)
{
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot         = core_spi_host();
    host.max_freq_khz = 5000;   /* conservative: fine on real cards and in simulation */

    sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot.host_id = core_spi_host();
    slot.gpio_cs = PIN_SD_CS;

    esp_vfs_fat_sdmmc_mount_config_t mount = {
        .format_if_mount_failed = true,
        .max_files              = 2,
        .allocation_unit_size   = 16 * 1024,
    };

    esp_err_t err = esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &host, &slot, &mount, &s_card);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "SD card not mounted (%s); logging to RAM only", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "SD card mounted at %s", SD_MOUNT_POINT);
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
/* CSV helpers                                                         */
/* ------------------------------------------------------------------ */

static void event_to_line(const event_t *e, char *line, size_t len)
{
    char room[4] = "-";
    if (e->room < ROOM_COUNT) {
        snprintf(room, sizeof room, "%d", e->room + 1);
    }
    snprintf(line, len, "%lld,%s,%08lX,%s,%s\n",
             (long long)e->ts, core_event_name(e->type), (unsigned long)e->uid, room, e->note);
}

static event_type_t type_from_name(const char *name)
{
    for (int t = 0; t < EV_TYPE_COUNT; t++) {
        if (strcmp(name, core_event_name(t)) == 0) {
            return t;
        }
    }
    return EV_TYPE_COUNT;
}

static bool line_to_event(const char *line, event_t *e)
{
    long long ts   = 0;
    char type[12]  = { 0 };
    unsigned long uid = 0;
    char room[4]   = { 0 };
    char note[24]  = { 0 };

    int fields = sscanf(line, "%lld,%11[^,],%lx,%3[^,],%23[^\n]", &ts, type, &uid, room, note);
    if (fields < 4) {
        return false;
    }
    memset(e, 0, sizeof *e);
    e->ts   = (time_t)ts;
    e->type = type_from_name(type);
    e->uid  = uid;
    e->room = ROOM_COUNT;
    if (room[0] >= '1' && room[0] <= '9') {
        e->room = (room_id_t)(room[0] - '1');
    }
    strlcpy(e->note, note, sizeof e->note);
    return e->type != EV_TYPE_COUNT;
}

/* ------------------------------------------------------------------ */
/* writing                                                             */
/* ------------------------------------------------------------------ */

static void ram_append(const event_t *e)
{
    s_ram[s_ram_head] = *e;
    s_ram_head = (s_ram_head + 1) % RAM_LOG_LEN;
    if (s_ram_count < RAM_LOG_LEN) {
        s_ram_count++;
    }
}

static void sd_append(const event_t *e)
{
    char line[80];
    event_to_line(e, line, sizeof line);

    FILE *f = fopen(LOG_FILE_PATH, "a");
    if (f == NULL) {
        ESP_LOGE(TAG, "cannot open %s for append", LOG_FILE_PATH);
        return;
    }
    fputs(line, f);
    fclose(f);   /* close after every line so a reset never loses a record */
}

/* ------------------------------------------------------------------ */
/* reading                                                             */
/* ------------------------------------------------------------------ */

static int ram_read_last(int n, event_t *out)
{
    if (n > s_ram_count) {
        n = s_ram_count;
    }
    /* oldest wanted entry sits n slots behind the head */
    int idx = (s_ram_head - n + RAM_LOG_LEN) % RAM_LOG_LEN;
    for (int i = 0; i < n; i++) {
        out[i] = s_ram[idx];
        idx = (idx + 1) % RAM_LOG_LEN;
    }
    return n;
}

static int sd_count_lines(void)
{
    FILE *f = fopen(LOG_FILE_PATH, "r");
    if (f == NULL) {
        return 0;
    }
    int lines = 0;
    char line[96];
    while (fgets(line, sizeof line, f) != NULL) {
        lines++;
    }
    fclose(f);
    return lines;
}

static int sd_read_last(int n, event_t *out)
{
    int total = sd_count_lines();
    int skip  = total - n;
    if (skip < 0) {
        skip = 0;
    }

    FILE *f = fopen(LOG_FILE_PATH, "r");
    if (f == NULL) {
        return 0;
    }
    int copied = 0;
    char line[96];
    int line_no = 0;
    while (fgets(line, sizeof line, f) != NULL && copied < n) {
        line_no++;
        if (line_no <= skip) {
            continue;
        }
        if (line_to_event(line, &out[copied])) {
            copied++;
        }
    }
    fclose(f);
    return copied;
}

/* ------------------------------------------------------------------ */
/* public API                                                          */
/* ------------------------------------------------------------------ */

esp_err_t storage_init(void)
{
    s_file_lock = xSemaphoreCreateMutex();
    if (s_file_lock == NULL) {
        ESP_LOGE(TAG, "could not create file mutex");
        return ESP_ERR_NO_MEM;
    }

    s_mounted = (sd_mount() == ESP_OK);

    BaseType_t ok = xTaskCreate(storage_task, "storage", 4096, NULL, 3, NULL);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "could not start logger task");
        return ESP_FAIL;
    }
    return ESP_OK;
}

int storage_read_last(int n, event_t *out)
{
    if (n <= 0 || out == NULL) {
        return 0;
    }
    xSemaphoreTake(s_file_lock, portMAX_DELAY);
    int copied;
    if (s_mounted) {
        copied = sd_read_last(n, out);
    } else {
        copied = ram_read_last(n, out);
    }
    xSemaphoreGive(s_file_lock);
    return copied;
}

int storage_count(event_type_t type)
{
    int count = 0;
    xSemaphoreTake(s_file_lock, portMAX_DELAY);
    if (s_mounted) {
        FILE *f = fopen(LOG_FILE_PATH, "r");
        if (f != NULL) {
            char line[96];
            event_t e;
            while (fgets(line, sizeof line, f) != NULL) {
                if (line_to_event(line, &e) && e.type == type) {
                    count++;
                }
            }
            fclose(f);
        }
    } else {
        for (int i = 0; i < s_ram_count; i++) {
            if (s_ram[i].type == type) {
                count++;
            }
        }
    }
    xSemaphoreGive(s_file_lock);
    return count;
}

bool storage_sd_mounted(void)
{
    return s_mounted;
}

/* ------------------------------------------------------------------ */
/* logger task: the single consumer of the event queue                 */
/* ------------------------------------------------------------------ */

static void storage_task(void *arg)
{
    QueueHandle_t q = core_event_queue();
    event_t e;

    while (1) {
        /* Blocks here until someone calls core_emit(); no polling, no CPU waste. */
        if (xQueueReceive(q, &e, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        xSemaphoreTake(s_file_lock, portMAX_DELAY);
        ram_append(&e);
        if (s_mounted) {
            sd_append(&e);
        }
        xSemaphoreGive(s_file_lock);
        ESP_LOGI(TAG, "logged %s uid=%08lX %s", core_event_name(e.type), (unsigned long)e.uid, e.note);
    }
}
