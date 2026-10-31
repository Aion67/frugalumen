/*
 * Lesson 03 — turn an event_t into a CSV line and back.
 *
 * The log file is plain text so a human can open it. One event per line:
 *   <unix time>,<TYPE>,<uid hex>,<room or ->,<note>
 * snprintf builds the line; sscanf takes it apart. The tricky formats:
 *   %11[^,]   up to 11 characters that are not a comma  (the type name)
 *   %lx       hex number into an unsigned long           (the uid)
 *   %23[^\n]  the rest of the line                       (the note)
 * No card needed for this lesson: it only uses strings.
 */
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "core.h"

static const char *TAG = "lesson03";

static void event_to_line(const event_t *e, char *line, size_t len)
{
    char room[4] = "-";
    if (e->room < ROOM_COUNT) {
        snprintf(room, sizeof room, "%d", e->room + 1);
    }
    snprintf(line, len, "%lld,%s,%08lX,%s,%s",
             (long long)e->ts, core_event_name(e->type), (unsigned long)e->uid, room, e->note);
}

static bool line_to_event(const char *line, event_t *e)
{
    long long ts = 0;
    char type[12] = { 0 };
    unsigned long uid = 0;
    char room[4] = { 0 };
    char note[24] = { 0 };
    int fields = sscanf(line, "%lld,%11[^,],%lx,%3[^,],%23[^\n]", &ts, type, &uid, room, note);
    if (fields < 4) {
        return false;
    }
    memset(e, 0, sizeof *e);
    e->ts  = (time_t)ts;
    e->uid = uid;
    e->room = ROOM_COUNT;
    if (room[0] >= '1' && room[0] <= '9') {
        e->room = (room_id_t)(room[0] - '1');
    }
    strlcpy(e->note, note, sizeof e->note);
    e->type = EV_TYPE_COUNT;
    for (int t = 0; t < EV_TYPE_COUNT; t++) {
        if (strcmp(type, core_event_name(t)) == 0) {
            e->type = t;
        }
    }
    return e->type != EV_TYPE_COUNT;
}

void app_main(void)
{
    event_t in = { .ts = 1700000000, .type = EV_LIGHT_ON, .uid = 0, .room = ROOM_2, .note = "room 2 on" };
    char line[80];
    event_to_line(&in, line, sizeof line);
    ESP_LOGI(TAG, "as text : %s", line);

    event_t out;
    if (line_to_event(line, &out)) {
        ESP_LOGI(TAG, "parsed  : ts=%lld type=%s room=%d note='%s'",
                 (long long)out.ts, core_event_name(out.type), out.room + 1, out.note);
    }

    ESP_LOGI(TAG, "garbage parses? %s", line_to_event("hello,world", &out) ? "yes (bug)" : "no (good)");
}
