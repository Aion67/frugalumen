/* Baseline implementation so every sandbox compiles on day one.
 * Joshua: replace the body with the real thing (NVS persistence, whitelist, anti-passback edge cases). */
#include <string.h>
#include "core.h"
#include "config.h"
#include "esp_log.h"
#include "freertos/semphr.h"

static const char *TAG = "core";

static SemaphoreHandle_t s_lock;
static QueueHandle_t     s_events;
static struct { uint32_t uid; bool inside; } s_people[MAX_PEOPLE];
static int      s_people_n;
static bool     s_room_occ[ROOM_COUNT];
static uint16_t s_sched_start = 8 * 60, s_sched_end = 18 * 60;

/* TODO(Joshua): load from NVS/SD instead of a fixed table */
static const struct { uint32_t uid; const char *name; } s_whitelist[] = {
    { 0xDEADBEEF, "Joan Nakato" }, { 0x12345678, "Dr. Nabukenya" },
};

#define LOCK()   xSemaphoreTake(s_lock, portMAX_DELAY)
#define UNLOCK() xSemaphoreGive(s_lock)

esp_err_t core_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    s_events = xQueueCreate(32, sizeof(event_t));
    if (!s_lock || !s_events) return ESP_ERR_NO_MEM;
    ESP_LOGI(TAG, "ready (schedule %02u:%02u-%02u:%02u)",
             s_sched_start / 60, s_sched_start % 60, s_sched_end / 60, s_sched_end % 60);
    return ESP_OK;
}

bool core_access_window_open(time_t now)
{
    struct tm t; localtime_r(&now, &t);
    uint16_t m = t.tm_hour * 60 + t.tm_min;
    LOCK(); bool ok = (m >= s_sched_start && m < s_sched_end); UNLOCK();
    return ok;
}

static int find(uint32_t uid)
{
    for (int i = 0; i < s_people_n; i++) if (s_people[i].uid == uid) return i;
    return -1;
}

esp_err_t core_record_entry(uint32_t uid, time_t now)
{
    LOCK();
    int i = find(uid);
    if (i >= 0 && s_people[i].inside) { UNLOCK(); return ESP_ERR_INVALID_STATE; }
    if (i < 0) { if (s_people_n >= MAX_PEOPLE) { UNLOCK(); return ESP_ERR_NO_MEM; } i = s_people_n++; s_people[i].uid = uid; }
    s_people[i].inside = true;
    UNLOCK();
    event_t e = { .ts = now, .type = EV_ENTRY, .uid = uid, .room = ROOM_COUNT, .note = "entry" };
    core_emit(&e);
    return ESP_OK;
}

esp_err_t core_record_exit(uint32_t uid, time_t now)
{
    LOCK();
    int i = find(uid);
    if (i < 0 || !s_people[i].inside) { UNLOCK(); return ESP_ERR_INVALID_STATE; }
    s_people[i].inside = false;
    UNLOCK();
    event_t e = { .ts = now, .type = EV_EXIT, .uid = uid, .room = ROOM_COUNT, .note = "exit" };
    core_emit(&e);
    return ESP_OK;
}

bool core_uid_known(uint32_t uid, char *name_out, size_t len)
{
    for (size_t i = 0; i < sizeof s_whitelist / sizeof s_whitelist[0]; i++)
        if (s_whitelist[i].uid == uid) { if (name_out) strlcpy(name_out, s_whitelist[i].name, len); return true; }
    return false;
}

void core_set_room_occupied(room_id_t r, bool occupied) { if (r < ROOM_COUNT) { LOCK(); s_room_occ[r] = occupied; UNLOCK(); } }
bool core_room_occupied(room_id_t r) { if (r >= ROOM_COUNT) return false; LOCK(); bool v = s_room_occ[r]; UNLOCK(); return v; }

int core_people_inside(void)
{
    LOCK(); int n = 0; for (int i = 0; i < s_people_n; i++) n += s_people[i].inside; UNLOCK();
    return n;
}

int core_list_inside(uint32_t *uids, int max)
{
    LOCK(); int n = 0;
    for (int i = 0; i < s_people_n && n < max; i++) if (s_people[i].inside) uids[n++] = s_people[i].uid;
    UNLOCK();
    return n;
}

esp_err_t core_schedule_set(uint16_t start_min, uint16_t end_min)
{
    if (start_min >= end_min || end_min > 24 * 60) return ESP_ERR_INVALID_ARG;
    LOCK(); s_sched_start = start_min; s_sched_end = end_min; UNLOCK();
    /* TODO(Joshua): persist to NVS */
    event_t e = { .ts = time(NULL), .type = EV_CONFIG, .uid = 0, .room = ROOM_COUNT, .note = "schedule" };
    core_emit(&e);
    return ESP_OK;
}

void core_schedule_get(uint16_t *start_min, uint16_t *end_min)
{
    LOCK(); if (start_min) *start_min = s_sched_start; if (end_min) *end_min = s_sched_end; UNLOCK();
}

void core_emit(const event_t *e) { if (s_events) xQueueSend(s_events, e, 0); }
QueueHandle_t core_event_queue(void) { return s_events; }
