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
