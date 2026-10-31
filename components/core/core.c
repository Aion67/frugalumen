/*
 * core.c — shared state for the whole system.
 *
 * Three jobs:
 *   1. Keep the one copy of "who is inside", room occupancy and the schedule,
 *      protected by a single mutex (requirement 2.7).
 *   2. Own the event queue that every module writes to and storage drains.
 *   3. Own things that are genuinely shared: the SPI and I2C buses, the RTC,
 *      and the NVS-backed schedule.
 *
 * Discipline: take the mutex, read or change the state, give it back at once.
 * Nothing slow (I/O, logging to SD, delays) happens while the mutex is held.
 */
#include <string.h>
#include <sys/time.h>
#include "core.h"
#include "config.h"
#include "rtc_ds1307.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "core";

/* ---- private state: only this file may touch these ---- */
typedef struct {
    uint32_t uid;
    bool     inside;
} person_t;

static SemaphoreHandle_t      s_lock;
static QueueHandle_t          s_events;
static person_t               s_people[MAX_PEOPLE];
static int                    s_people_count;
static bool                   s_room_occupied[ROOM_COUNT];
static uint16_t               s_sched_start = SCHEDULE_DEFAULT_START_MIN;
static uint16_t               s_sched_end   = SCHEDULE_DEFAULT_END_MIN;
static i2c_master_bus_handle_t s_i2c_bus;
static bool                   s_rtc_ok;

/* Known tags. UIDs match the Wokwi MFRC522 card presets so the demo needs no setup.
 * The red card (AA:BB:CC:DD) is deliberately left out to demonstrate a denial. */
typedef struct {
    uint32_t    uid;
    const char *name;
} known_tag_t;

static const known_tag_t s_known_tags[] = {
    { 0x01020304, "Joan Nakato (student)"     },   /* blue card   */
    { 0x11223344, "Peter Okello (student)"    },   /* green card  */
    { 0x55667788, "Dr. Nabukenya (lecturer)"  },   /* yellow card */
    { 0xC0FFEE99, "Night guard (key fob)"     },   /* key fob     */
};
#define KNOWN_TAG_COUNT (sizeof s_known_tags / sizeof s_known_tags[0])

static const char *s_event_names[EV_TYPE_COUNT] = {
    "ENTRY", "EXIT", "DENIED", "LIGHT_ON", "LIGHT_OFF", "CONFIG", "ALERT",
};

static void core_task(void *arg);

/* ------------------------------------------------------------------ */
/* small private helpers                                               */
/* ------------------------------------------------------------------ */

static void lock(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
}

static void unlock(void)
{
    xSemaphoreGive(s_lock);
}

/* Must be called with the lock held. Returns index or -1. */
static int find_person(uint32_t uid)
{
    for (int i = 0; i < s_people_count; i++) {
        if (s_people[i].uid == uid) {
            return i;
        }
    }
    return -1;
}

static uint16_t minutes_of_day(time_t now)
{
    struct tm t;
    localtime_r(&now, &t);
    return (uint16_t)(t.tm_hour * 60 + t.tm_min);
}

static void emit_simple(event_type_t type, uint32_t uid, const char *note)
{
    event_t e = { 0 };
    e.ts   = time(NULL);
    e.type = type;
    e.uid  = uid;
    e.room = ROOM_COUNT;
    strlcpy(e.note, note, sizeof e.note);
    core_emit(&e);
}

/* ------------------------------------------------------------------ */
/* NVS: the schedule must survive a reset                              */
/* ------------------------------------------------------------------ */

static esp_err_t nvs_setup(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        /* Partition layout changed; wipe and start fresh. Only happens in development. */
        ESP_LOGW(TAG, "NVS partition needs erasing");
        err = nvs_flash_erase();
        if (err != ESP_OK) {
            return err;
        }
        err = nvs_flash_init();
    }
    return err;
}

static void schedule_load(void)
{
    nvs_handle_t h;
    if (nvs_open("cocis", NVS_READONLY, &h) != ESP_OK) {
        ESP_LOGI(TAG, "no saved schedule, using defaults");
        return;
    }
    uint16_t start = 0;
    uint16_t end   = 0;
    if (nvs_get_u16(h, "sch_start", &start) == ESP_OK &&
        nvs_get_u16(h, "sch_end", &end) == ESP_OK) {
        s_sched_start = start;
        s_sched_end   = end;
        ESP_LOGI(TAG, "loaded schedule from NVS");
    }
    nvs_close(h);
}

static esp_err_t schedule_save(uint16_t start, uint16_t end)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open("cocis", NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed");
        return err;
    }
    nvs_set_u16(h, "sch_start", start);
    nvs_set_u16(h, "sch_end", end);
    err = nvs_commit(h);
    nvs_close(h);
    return err;
}

/* ------------------------------------------------------------------ */
/* buses and clock                                                     */
/* ------------------------------------------------------------------ */

static esp_err_t buses_setup(void)
{
    spi_bus_config_t spi = {
        .sclk_io_num     = PIN_SPI_SCK,
        .mosi_io_num     = PIN_SPI_MOSI,
        .miso_io_num     = PIN_SPI_MISO,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = 4096,
    };
    esp_err_t err = spi_bus_initialize(core_spi_host(), &spi, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus init failed");
        return err;
    }

    i2c_master_bus_config_t i2c = {
        .i2c_port          = I2C_NUM_0,
        .sda_io_num        = PIN_I2C_SDA,
        .scl_io_num        = PIN_I2C_SCL,
        .clk_source        = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    err = i2c_new_master_bus(&i2c, &s_i2c_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus init failed");
        return err;
    }
    return ESP_OK;
}

static void clock_setup(void)
{
    /* The RTC holds plain wall-clock time; tell libc not to apply any offset. */
    setenv("TZ", "UTC0", 1);
    tzset();

    if (rtc_ds1307_init(s_i2c_bus) != ESP_OK) {
        return;
    }
    time_t now = 0;
    if (rtc_ds1307_read(&now) != ESP_OK) {
        ESP_LOGW(TAG, "RTC unreadable; clock starts at 1970. Use the console to set it.");
        return;
    }
    struct timeval tv = { .tv_sec = now, .tv_usec = 0 };
    settimeofday(&tv, NULL);
    s_rtc_ok = true;

    char buf[32];
    struct tm t;
    localtime_r(&now, &t);
    strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", &t);
    ESP_LOGI(TAG, "clock set from RTC: %s", buf);
}

/* ------------------------------------------------------------------ */
/* public API                                                          */
/* ------------------------------------------------------------------ */

esp_err_t core_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    if (s_lock == NULL) {
        ESP_LOGE(TAG, "could not create mutex");
        return ESP_ERR_NO_MEM;
    }
    s_events = xQueueCreate(EVENT_QUEUE_LEN, sizeof(event_t));
    if (s_events == NULL) {
        ESP_LOGE(TAG, "could not create event queue");
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = nvs_setup();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed");
        return err;
    }
    schedule_load();

    err = buses_setup();
    if (err != ESP_OK) {
        return err;
    }
    clock_setup();

    BaseType_t ok = xTaskCreate(core_task, "core", 3072, NULL, 4, NULL);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "could not start core task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "ready, access window %02u:%02u-%02u:%02u",
             s_sched_start / 60, s_sched_start % 60, s_sched_end / 60, s_sched_end % 60);
    return ESP_OK;
}

bool core_access_window_open(time_t now)
{
    uint16_t minute = minutes_of_day(now);
    lock();
    bool open = (minute >= s_sched_start) && (minute < s_sched_end);
    unlock();
    return open;
}

esp_err_t core_record_entry(uint32_t uid, time_t now)
{
    lock();
    int i = find_person(uid);
    if (i >= 0 && s_people[i].inside) {
        unlock();
        return ESP_ERR_INVALID_STATE;       /* anti-passback: already inside */
    }
    if (i < 0) {
        if (s_people_count >= MAX_PEOPLE) {
            unlock();
            ESP_LOGE(TAG, "people table full");
            return ESP_ERR_NO_MEM;
        }
        i = s_people_count;
        s_people_count++;
        s_people[i].uid = uid;
    }
    s_people[i].inside = true;
    unlock();

    event_t e = { .ts = now, .type = EV_ENTRY, .uid = uid, .room = ROOM_COUNT, .note = "entry" };
    core_emit(&e);
    return ESP_OK;
}

esp_err_t core_record_exit(uint32_t uid, time_t now)
{
    lock();
    int i = find_person(uid);
    if (i < 0 || !s_people[i].inside) {
        unlock();
        return ESP_ERR_INVALID_STATE;       /* anti-passback: never entered */
    }
    s_people[i].inside = false;
    unlock();

    event_t e = { .ts = now, .type = EV_EXIT, .uid = uid, .room = ROOM_COUNT, .note = "exit" };
    core_emit(&e);
    return ESP_OK;
}

bool core_uid_known(uint32_t uid, char *name_out, size_t len)
{
    /* The table is constant, so no lock is needed to read it. */
    for (size_t i = 0; i < KNOWN_TAG_COUNT; i++) {
        if (s_known_tags[i].uid == uid) {
            if (name_out != NULL) {
                strlcpy(name_out, s_known_tags[i].name, len);
            }
            return true;
        }
    }
    if (name_out != NULL && len > 0) {
        name_out[0] = '\0';
    }
    return false;
}

void core_set_room_occupied(room_id_t r, bool occupied)
{
    if (r >= ROOM_COUNT) {
        return;
    }
    lock();
    s_room_occupied[r] = occupied;
    unlock();
}

bool core_room_occupied(room_id_t r)
{
    if (r >= ROOM_COUNT) {
        return false;
    }
    lock();
    bool v = s_room_occupied[r];
    unlock();
    return v;
}

int core_people_inside(void)
{
    int n = 0;
    lock();
    for (int i = 0; i < s_people_count; i++) {
        if (s_people[i].inside) {
            n++;
        }
    }
    unlock();
    return n;
}

int core_list_inside(uint32_t *uids, int max)
{
    int n = 0;
    lock();
    for (int i = 0; i < s_people_count && n < max; i++) {
        if (s_people[i].inside) {
            uids[n] = s_people[i].uid;
            n++;
        }
    }
    unlock();
    return n;
}

esp_err_t core_schedule_set(uint16_t start_min, uint16_t end_min)
{
    if (start_min >= end_min || end_min > 24 * 60) {
        return ESP_ERR_INVALID_ARG;
    }
    lock();
    s_sched_start = start_min;
    s_sched_end   = end_min;
    unlock();

    esp_err_t err = schedule_save(start_min, end_min);   /* slow: done outside the lock */
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "schedule changed but not saved to NVS");
    }
    emit_simple(EV_CONFIG, 0, "schedule changed");
    return ESP_OK;
}

void core_schedule_get(uint16_t *start_min, uint16_t *end_min)
{
    lock();
    if (start_min != NULL) {
        *start_min = s_sched_start;
    }
    if (end_min != NULL) {
        *end_min = s_sched_end;
    }
    unlock();
}

void core_emit(const event_t *e)
{
    if (s_events == NULL) {
        return;
    }
    /* Zero wait: a producer must never block because the SD card is slow. */
    if (xQueueSend(s_events, e, 0) != pdTRUE) {
        ESP_LOGW(TAG, "event queue full, %s dropped", core_event_name(e->type));
    }
}

QueueHandle_t core_event_queue(void)
{
    return s_events;
}

const char *core_event_name(event_type_t type)
{
    if (type >= EV_TYPE_COUNT) {
        return "?";
    }
    return s_event_names[type];
}

esp_err_t core_time_set(time_t now)
{
    struct timeval tv = { .tv_sec = now, .tv_usec = 0 };
    settimeofday(&tv, NULL);
    emit_simple(EV_CONFIG, 0, "time set");
    if (!s_rtc_ok) {
        ESP_LOGW(TAG, "system clock set; RTC not available");
        return ESP_OK;
    }
    return rtc_ds1307_write(now);
}

spi_host_device_t core_spi_host(void)
{
    return SPI2_HOST;
}

i2c_master_bus_handle_t core_i2c_bus(void)
{
    return s_i2c_bus;
}

/* ------------------------------------------------------------------ */
/* core task: watches for the access window closing with people inside */
/* ------------------------------------------------------------------ */

static void core_task(void *arg)
{
    bool was_open = core_access_window_open(time(NULL));

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(OVERSTAY_CHECK_MS));

        bool open = core_access_window_open(time(NULL));
        if (was_open && !open) {
            int inside = core_people_inside();
            if (inside > 0) {
                char note[24];
                snprintf(note, sizeof note, "overstay:%d", inside);
                ESP_LOGW(TAG, "access window closed with %d people inside", inside);
                emit_simple(EV_ALERT, 0, note);
            }
        }
        was_open = open;
    }
}
