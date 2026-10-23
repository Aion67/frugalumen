#!/usr/bin/env bash
# Scaffold the frugalumen repo layout (CoCIS Smart Control). Run from the repo root.
# Safe to re-run: never overwrites a file that already exists.
set -euo pipefail

MODULES="core rfid_door lighting storage console display"
# name:module pairs for sandboxes
SANDBOXES="joshua:core jovia:rfid_door shirley:lighting hairat:storage prince:console joel:display"

mk() { # mk <path>  (reads content from stdin, skips if exists)
  if [ -e "$1" ]; then
    echo "  keep   $1"
    cat >/dev/null
  else
    mkdir -p "$(dirname "$1")"
    cat >"$1"
    echo "  create $1"
  fi
}

echo "== root"
mk CMakeLists.txt <<'EOF'
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(frugalumen)
EOF

mk .gitignore <<'EOF'
build/
**/build/
sdkconfig.old
managed_components/
dependencies.lock
*.uf2
.vscode/
EOF

mk wokwi.toml <<'EOF'
[wokwi]
version = 1
firmware = "build/flasher_args.json"
elf = "build/frugalumen.elf"
EOF

mk diagram.json <<'EOF'
{
  "version": 1,
  "author": "Group 4",
  "editor": "wokwi",
  "parts": [
    { "type": "board-esp32-devkit-c-v4", "id": "esp", "top": 0, "left": 0, "attrs": {} }
  ],
  "connections": [ [ "esp:TX", "$serialMonitor:RX", "", [] ], [ "esp:RX", "$serialMonitor:TX", "", [] ] ]
}
EOF

echo "== main (integration project — Joshua)"
mk main/CMakeLists.txt <<'EOF'
idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES core rfid_door lighting storage console display)
EOF

mk main/main.c <<'EOF'
#include "esp_log.h"
#include "core.h"
#include "config.h"
#include "rfid_door.h"
#include "lighting.h"
#include "storage.h"
#include "console.h"
#include "display.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "frugalumen starting");
    ESP_ERROR_CHECK(core_init());
    ESP_ERROR_CHECK(storage_init());      // logger first so nothing is lost
    ESP_ERROR_CHECK(rfid_door_init());
    ESP_ERROR_CHECK(lighting_init(ROOM_1, PIN_PIR_ROOM1, PIN_LED_ROOM1));
    ESP_ERROR_CHECK(lighting_init(ROOM_2, PIN_PIR_ROOM2, PIN_LED_ROOM2));
    ESP_ERROR_CHECK(display_init());
    ESP_ERROR_CHECK(console_init());
    ESP_LOGI(TAG, "all modules up");
}
EOF

echo "== components/core (the contract)"
mk components/core/CMakeLists.txt <<'EOF'
idf_component_register(SRCS "core.c"
                       INCLUDE_DIRS "include"
                       REQUIRES freertos nvs_flash driver)
EOF

mk components/core/include/config.h <<'EOF'
#pragma once
/* Single source of truth for pins and timing. Owned by Joshua. Ask before editing. */
#include "driver/gpio.h"

/* SPI bus (shared by RFID reader and SD card) */
#define PIN_SPI_SCK        GPIO_NUM_18
#define PIN_SPI_MOSI       GPIO_NUM_23
#define PIN_SPI_MISO       GPIO_NUM_19
#define PIN_RFID_CS        GPIO_NUM_5
#define PIN_RFID_RST       GPIO_NUM_27
#define PIN_SD_CS          GPIO_NUM_15

/* I2C bus (LCD + DS1307 RTC) */
#define PIN_I2C_SDA        GPIO_NUM_21
#define PIN_I2C_SCL        GPIO_NUM_22

/* Entrance */
#define PIN_SERVO_ENTRY    GPIO_NUM_25
#define PIN_SERVO_EXIT     GPIO_NUM_26
#define PIN_BTN_LANE       GPIO_NUM_16   /* selects entry/exit lane on the single simulated reader */
#define PIN_BUZZER         GPIO_NUM_2

/* Rooms */
#define PIN_PIR_ROOM1      GPIO_NUM_32
#define PIN_LED_ROOM1      GPIO_NUM_14
#define PIN_PIR_ROOM2      GPIO_NUM_33
#define PIN_LED_ROOM2      GPIO_NUM_13
#define PIN_BTN_OVERRIDE   GPIO_NUM_17

/* Timing */
#define DOOR_OPEN_MS       3000
#define LIGHT_GRACE_MS     10000
#define DISPLAY_REFRESH_MS 1000

/* Admin */
#define ADMIN_PIN          "2468"
#define MAX_PEOPLE         64
EOF

mk components/core/include/core.h <<'EOF'
#pragma once
/* Shared-state contract. Frozen on Saturday. Every module compiles against this and nothing else. */
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef enum { ROOM_1 = 0, ROOM_2, ROOM_COUNT } room_id_t;
typedef enum { LANE_ENTRY, LANE_EXIT } lane_t;
typedef enum { EV_ENTRY, EV_EXIT, EV_DENIED, EV_LIGHT_ON, EV_LIGHT_OFF,
               EV_CONFIG, EV_ALERT } event_type_t;

typedef struct {
    time_t        ts;
    event_type_t  type;
    uint32_t      uid;        /* 0 if not a person event */
    room_id_t     room;       /* ROOM_COUNT if not a room event */
    char          note[24];
} event_t;

esp_err_t core_init(void);

/* access control — called by rfid_door */
bool      core_access_window_open(time_t now);
esp_err_t core_record_entry(uint32_t uid, time_t now);   /* ESP_ERR_INVALID_STATE if already inside */
esp_err_t core_record_exit (uint32_t uid, time_t now);   /* ESP_ERR_INVALID_STATE if not inside */
bool      core_uid_known(uint32_t uid, char *name_out, size_t len);

/* occupancy — set by lighting, read by console/display */
void      core_set_room_occupied(room_id_t r, bool occupied);
bool      core_room_occupied(room_id_t r);
int       core_people_inside(void);
int       core_list_inside(uint32_t *uids, int max);

/* schedule — set by console (after auth), persisted in NVS */
esp_err_t core_schedule_set(uint16_t start_min, uint16_t end_min);   /* minutes since midnight */
void      core_schedule_get(uint16_t *start_min, uint16_t *end_min);

/* events — any module may emit; storage consumes */
void          core_emit(const event_t *e);
QueueHandle_t core_event_queue(void);
EOF

mk components/core/core.c <<'EOF'
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
EOF

echo "== components (one per module)"
for m in rfid_door lighting storage console display; do
  case $m in
  rfid_door)
    sig='esp_err_t rfid_door_init(void);'
    req='core driver'
    ;;
  lighting)
    sig='esp_err_t lighting_init(room_id_t room, gpio_num_t pir_pin, gpio_num_t led_pin);'
    req='core driver'
    ;;
  storage)
    sig='esp_err_t storage_init(void);
int       storage_read_last(int n, event_t *out);
int       storage_count(event_type_t type);'
    req='core driver fatfs sdmmc'
    ;;
  console)
    sig='esp_err_t console_init(void);'
    req='core console nvs_flash'
    ;;
  display)
    sig='esp_err_t display_init(void);'
    req='core driver'
    ;;
  esac

  mk "components/$m/CMakeLists.txt" <<EOF
idf_component_register(SRCS "$m.c"
                       INCLUDE_DIRS "include"
                       REQUIRES $req)
EOF

  mk "components/$m/include/$m.h" <<EOF
#pragma once
#include "esp_err.h"
#include "core.h"
#include "driver/gpio.h"

$sig
EOF

  mk "components/$m/$m.c" <<EOF
#include "$m.h"
#include "config.h"
#include "esp_log.h"

static const char *TAG = "$m";

/* TODO(owner): real implementation. This stub only proves the module links. */
EOF
  # append a matching stub body per signature
  case $m in
  rfid_door)
    cat >>"components/$m/$m.c" <<'EOF'
esp_err_t rfid_door_init(void) { ESP_LOGW(TAG, "stub"); return ESP_OK; }
EOF
    ;;
  lighting)
    cat >>"components/$m/$m.c" <<'EOF'
esp_err_t lighting_init(room_id_t room, gpio_num_t pir_pin, gpio_num_t led_pin)
{ (void)pir_pin; (void)led_pin; ESP_LOGW(TAG, "stub room %d", room); return ESP_OK; }
EOF
    ;;
  storage)
    cat >>"components/$m/$m.c" <<'EOF'
esp_err_t storage_init(void) { ESP_LOGW(TAG, "stub"); return ESP_OK; }
int storage_read_last(int n, event_t *out) { (void)n; (void)out; return 0; }
int storage_count(event_type_t type) { (void)type; return 0; }
EOF
    ;;
  console)
    cat >>"components/$m/$m.c" <<'EOF'
esp_err_t console_init(void) { ESP_LOGW(TAG, "stub"); return ESP_OK; }
EOF
    ;;
  display)
    cat >>"components/$m/$m.c" <<'EOF'
esp_err_t display_init(void) { ESP_LOGW(TAG, "stub"); return ESP_OK; }
EOF
    ;;
  esac

  mk "components/$m/README.md" <<EOF
# $m

**Owner:**
**Brief section:**

## What it does

## Pins used (see config.h)

## Public API
See \`include/$m.h\`.

## How it was tested
Sandbox: \`sandbox/<name>-$m/\`
Wokwi link:

## Known limits
EOF
done

echo "== sandboxes (one per person)"
for pair in $SANDBOXES; do
  name=${pair%%:*}
  mod=${pair##*:}
  dir="sandbox/$name-$mod"

  mk "$dir/CMakeLists.txt" <<EOF
cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS ../../components)
include(\$ENV{IDF_PATH}/tools/cmake/project.cmake)
project(sandbox_${name}_${mod})
EOF

  mk "$dir/main/CMakeLists.txt" <<EOF
idf_component_register(SRCS "main.c" INCLUDE_DIRS "." REQUIRES core $mod)
EOF

  mk "$dir/main/main.c" <<EOF
/* Test harness for $mod. Owner: $name. Keep this small; it exists to prove the module works alone. */
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "core.h"
#include "config.h"
#include "$mod.h"

static const char *TAG = "sandbox";

void app_main(void)
{
    ESP_ERROR_CHECK(core_init());
    /* TODO($name): call ${mod}_init(...) with the right arguments, then exercise it */
    ESP_LOGI(TAG, "$mod sandbox running");
    for (;;) {
        ESP_LOGI(TAG, "people inside: %d", core_people_inside());
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
EOF

  mk "$dir/wokwi.toml" <<EOF
[wokwi]
version = 1
firmware = "build/flasher_args.json"
elf = "build/sandbox_${name}_${mod}.elf"
EOF

  mk "$dir/diagram.json" <<'EOF'
{
  "version": 1,
  "editor": "wokwi",
  "parts": [
    { "type": "board-esp32-devkit-c-v4", "id": "esp", "top": 0, "left": 0, "attrs": {} }
  ],
  "connections": [ [ "esp:TX", "$serialMonitor:RX", "", [] ], [ "esp:RX", "$serialMonitor:TX", "", [] ] ]
}
EOF
done

echo "== done"
echo
echo "Next: cd sandbox/joshua-core && idf.py set-target esp32 && idf.py build"
