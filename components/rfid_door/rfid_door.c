/*
 * rfid_door.c — the entrance terminal.
 *
 * One task polls both readers. When a tag appears it is checked in this order:
 *   1. Is the tag known?                       -> else DENIED "unknown"
 *   2. Is the access window open?              -> entry refused outside hours;
 *                                                exit allowed only for people
 *                                                already inside (overstay exit)
 *   3. Anti-passback: core refuses a second    -> DENIED "already inside" /
 *      entry or an exit without an entry          "not inside"
 * A pass opens that lane's door for DOOR_OPEN_MS and beeps once.
 *
 * Door timing is non-blocking: the task remembers when to close each door, so
 * a scan on the other lane is never delayed by an open door.
 */
#include "rfid_door.h"
#include "config.h"
#include "mfrc522.h"
#include "door_servo.h"
#include "buzzer.h"
#include "esp_log.h"
#include "freertos/task.h"

static const char *TAG = "rfid_door";

typedef struct {
    lane_t      lane;
    mfrc522_t   reader;
    uint32_t    last_uid;
    TickType_t  last_scan_tick;
    bool        door_open;
    TickType_t  close_tick;
} lane_state_t;

static lane_state_t s_lanes[2];

static void rfid_door_task(void *arg);

static const char *lane_name(lane_t lane)
{
    if (lane == LANE_ENTRY) {
        return "entry";
    }
    return "exit";
}

static void deny(lane_t lane, uint32_t uid, const char *reason)
{
    event_t e = { 0 };
    e.ts   = time(NULL);
    e.type = EV_DENIED;
    e.uid  = uid;
    e.room = ROOM_COUNT;
    snprintf(e.note, sizeof e.note, "%s: %s", lane_name(lane), reason);
    core_emit(&e);
    ESP_LOGW(TAG, "%s DENIED uid %08lX (%s)", lane_name(lane), (unsigned long)uid, reason);
    buzzer_beep_deny();
}

static void open_door(lane_state_t *ls)
{
    door_servo_set(ls->lane, true);
    ls->door_open  = true;
    ls->close_tick = xTaskGetTickCount() + pdMS_TO_TICKS(DOOR_OPEN_MS);
    buzzer_beep_ok();
}

static void handle_scan(lane_state_t *ls, uint32_t uid)
{
    char name[32];
    time_t now = time(NULL);

    if (!core_uid_known(uid, name, sizeof name)) {
        deny(ls->lane, uid, "unknown tag");
        return;
    }

    bool window_open = core_access_window_open(now);

    if (ls->lane == LANE_ENTRY) {
        if (!window_open) {
            deny(ls->lane, uid, "closed hours");
            return;
        }
        esp_err_t err = core_record_entry(uid, now);
        if (err == ESP_ERR_INVALID_STATE) {
            deny(ls->lane, uid, "already inside");
            return;
        }
        if (err != ESP_OK) {
            deny(ls->lane, uid, "system error");
            return;
        }
        ESP_LOGI(TAG, "ENTRY %s", name);
        open_door(ls);
        return;
    }

    /* exit lane */
    esp_err_t err = core_record_exit(uid, now);
    if (err == ESP_ERR_INVALID_STATE) {
        deny(ls->lane, uid, "not inside");
        return;
    }
    if (!window_open) {
        /* Innovation 2: people caught inside at closing may still leave, but it is flagged. */
        event_t alert = { .ts = now, .type = EV_ALERT, .uid = uid, .room = ROOM_COUNT,
                          .note = "late exit" };
        core_emit(&alert);
        ESP_LOGW(TAG, "EXIT %s after hours", name);
    } else {
        ESP_LOGI(TAG, "EXIT %s", name);
    }
    open_door(ls);
}

esp_err_t rfid_door_init(void)
{
    /* Both readers share one reset line; hold it high so they run. */
    gpio_config_t rst = {
        .pin_bit_mask = 1ULL << PIN_RFID_RST,
        .mode         = GPIO_MODE_OUTPUT,
    };
    esp_err_t err = gpio_config(&rst);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "reset pin config failed");
        return err;
    }
    gpio_set_level(PIN_RFID_RST, 1);

    s_lanes[LANE_ENTRY].lane = LANE_ENTRY;
    s_lanes[LANE_EXIT].lane  = LANE_EXIT;

    err = mfrc522_init(&s_lanes[LANE_ENTRY].reader, core_spi_host(), PIN_RFID_ENTRY_CS, "entry");
    if (err != ESP_OK) {
        return err;
    }
    err = mfrc522_init(&s_lanes[LANE_EXIT].reader, core_spi_host(), PIN_RFID_EXIT_CS, "exit");
    if (err != ESP_OK) {
        return err;
    }
    err = door_servo_init();
    if (err != ESP_OK) {
        return err;
    }
    err = buzzer_init();
    if (err != ESP_OK) {
        return err;
    }

    BaseType_t ok = xTaskCreate(rfid_door_task, "rfid_door", 4096, NULL, 6, NULL);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "could not start task");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "entrance ready");
    return ESP_OK;
}

static void poll_lane(lane_state_t *ls)
{
    TickType_t now = xTaskGetTickCount();

    if (ls->door_open && (int32_t)(now - ls->close_tick) >= 0) {
        door_servo_set(ls->lane, false);
        ls->door_open = false;
    }

    uint32_t uid = 0;
    if (!mfrc522_read_uid(&ls->reader, &uid)) {
        return;
    }

    /* A card held on the reader answers every poll; only act on it once. */
    bool same_card = (uid == ls->last_uid);
    bool recent    = (now - ls->last_scan_tick) < pdMS_TO_TICKS(RFID_REPEAT_MS);
    if (same_card && recent) {
        ls->last_scan_tick = now;
        return;
    }
    ls->last_uid       = uid;
    ls->last_scan_tick = now;

    handle_scan(ls, uid);
}

static void rfid_door_task(void *arg)
{
    while (1) {
        poll_lane(&s_lanes[LANE_ENTRY]);
        poll_lane(&s_lanes[LANE_EXIT]);
        vTaskDelay(pdMS_TO_TICKS(RFID_POLL_MS));
    }
}
