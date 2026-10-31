/*
 * console.c — the management menu on UART0.
 *
 * This is the one module allowed to printf: the serial monitor is its output.
 * Input is read one character at a time from the UART driver and echoed back,
 * because the Wokwi serial monitor does not echo what you type.
 *
 * Admin items (schedule, time, PIN) ask for the PIN every time; there is no
 * "logged in" state to forget to log out of.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "console.h"
#include "config.h"
#include "auth.h"
#include "storage.h"
#include "esp_log.h"
#include "esp_system.h"
#include "driver/uart.h"
#include "driver/uart_vfs.h"
#include "freertos/task.h"

static const char *TAG = "console";

#define CONSOLE_UART      UART_NUM_0
#define INPUT_MAX          40
#define LOG_VIEW_ROWS     10
#define REPORT_MAX_EVENTS 256

static void console_task(void *arg);

/* ------------------------------------------------------------------ */
/* line input                                                          */
/* ------------------------------------------------------------------ */

static void read_line(char *buf, size_t len)
{
    size_t n = 0;
    while (1) {
        uint8_t c;
        int got = uart_read_bytes(CONSOLE_UART, &c, 1, pdMS_TO_TICKS(100));
        if (got != 1) {
            continue;                                  /* nothing typed yet; keep waiting */
        }
        if (c == '\r' || c == '\n') {
            if (n == 0) {
                continue;                              /* ignore the LF after a CR */
            }
            break;
        }
        if (c == 0x08 || c == 0x7F) {                  /* backspace / delete */
            if (n > 0) {
                n--;
                printf("\b \b");
                fflush(stdout);
            }
            continue;
        }
        if (n < len - 1 && isprint(c)) {
            buf[n] = (char)c;
            n++;
            printf("%c", c);
            fflush(stdout);
        }
    }
    buf[n] = '\0';
    printf("\n");
}

static void prompt(const char *text, char *buf, size_t len)
{
    printf("%s", text);
    fflush(stdout);
    read_line(buf, len);
}

static bool admin_login(void)
{
    char pin[INPUT_MAX];
    prompt("Admin PIN: ", pin, sizeof pin);
    if (auth_check_pin(pin)) {
        return true;
    }
    printf("Wrong PIN. Access refused.\n");
    event_t e = { .ts = time(NULL), .type = EV_DENIED, .uid = 0, .room = ROOM_COUNT,
                  .note = "console: bad PIN" };
    core_emit(&e);
    return false;
}

/* "HH:MM" -> minutes since midnight, or -1 */
static int parse_hhmm(const char *s)
{
    int h = 0;
    int m = 0;
    if (sscanf(s, "%d:%d", &h, &m) != 2) {
        return -1;
    }
    if (h < 0 || h > 24 || m < 0 || m > 59) {
        return -1;
    }
    return h * 60 + m;
}

static void format_time(time_t ts, char *out, size_t len)
{
    struct tm t;
    localtime_r(&ts, &t);
    strftime(out, len, "%Y-%m-%d %H:%M:%S", &t);
}

static void format_duration(long seconds, char *out, size_t len)
{
    long h = seconds / 3600;
    long m = (seconds % 3600) / 60;
    long s = seconds % 60;
    snprintf(out, len, "%ld:%02ld:%02ld", h, m, s);
}

/* ------------------------------------------------------------------ */
/* menu items                                                          */
/* ------------------------------------------------------------------ */

static void show_menu(void)
{
    printf("\n=== CoCIS CONTROL ===\n");
    printf("1. Show occupancy\n");
    printf("2. Who is inside\n");
    printf("3. Show lights\n");
    printf("4. View access log\n");
    printf("5. Set schedule       [admin]\n");
    printf("6. Set time           [admin]\n");
    printf("7. Daily report\n");
    printf("8. System status\n");
    printf("9. Change admin PIN   [admin]\n");
    printf("0. Exit\n");
    printf("Select: ");
    fflush(stdout);
}

static void item_occupancy(void)
{
    printf("Building: %d people inside\n", core_people_inside());
    for (int r = 0; r < ROOM_COUNT; r++) {
        printf("Room %d:   %s\n", r + 1, core_room_occupied(r) ? "occupied" : "empty");
    }
}

static void item_who_inside(void)
{
    uint32_t uids[MAX_PEOPLE];
    int n = core_list_inside(uids, MAX_PEOPLE);
    if (n == 0) {
        printf("Nobody is inside.\n");
        return;
    }
    for (int i = 0; i < n; i++) {
        char name[32];
        core_uid_known(uids[i], name, sizeof name);
        printf("  %08lX  %s\n", (unsigned long)uids[i], name);
    }
}

static void item_lights(void)
{
    for (int r = 0; r < ROOM_COUNT; r++) {
        printf("Room %d light: %s\n", r + 1, core_room_occupied(r) ? "ON" : "OFF");
    }
}

static void item_access_log(void)
{
    event_t events[LOG_VIEW_ROWS];
    int n = storage_read_last(LOG_VIEW_ROWS, events);
    if (n == 0) {
        printf("Log is empty.\n");
        return;
    }
    printf("%-19s  %-9s  %-8s  %s\n", "time", "event", "uid", "note");
    for (int i = 0; i < n; i++) {
        char when[24];
        format_time(events[i].ts, when, sizeof when);
        printf("%s  %-9s  %08lX  %s\n", when, core_event_name(events[i].type),
               (unsigned long)events[i].uid, events[i].note);
    }
}

static void item_set_schedule(void)
{
    if (!admin_login()) {
        return;
    }
    char text[INPUT_MAX];
    prompt("Access starts at (HH:MM): ", text, sizeof text);
    int start = parse_hhmm(text);
    prompt("Access ends at   (HH:MM): ", text, sizeof text);
    int end = parse_hhmm(text);
    if (start < 0 || end < 0) {
        printf("Bad time format.\n");
        return;
    }
    esp_err_t err = core_schedule_set((uint16_t)start, (uint16_t)end);
    if (err != ESP_OK) {
        printf("Rejected: start must be before end.\n");
        return;
    }
    printf("Schedule saved: %02d:%02d - %02d:%02d\n", start / 60, start % 60, end / 60, end % 60);
}

static void item_set_time(void)
{
    if (!admin_login()) {
        return;
    }
    char text[INPUT_MAX];
    prompt("New time (YYYY-MM-DD HH:MM): ", text, sizeof text);

    struct tm t = { 0 };
    int fields = sscanf(text, "%d-%d-%d %d:%d", &t.tm_year, &t.tm_mon, &t.tm_mday, &t.tm_hour, &t.tm_min);
    if (fields != 5) {
        printf("Bad format.\n");
        return;
    }
    t.tm_year -= 1900;
    t.tm_mon  -= 1;
    time_t now = mktime(&t);
    if (core_time_set(now) != ESP_OK) {
        printf("Clock set, but the RTC did not accept it.\n");
        return;
    }
    printf("Clock set.\n");
}

static void item_daily_report(void)
{
    event_t *events = malloc(sizeof(event_t) * REPORT_MAX_EVENTS);
    if (events == NULL) {
        printf("Out of memory.\n");
        return;
    }
    int n = storage_read_last(REPORT_MAX_EVENTS, events);

    int entries = 0;
    int exits   = 0;
    int denied  = 0;
    int alerts  = 0;
    int inside  = 0;
    int peak    = 0;
    long   light_seconds[ROOM_COUNT] = { 0 };
    time_t light_on_since[ROOM_COUNT] = { 0 };

    for (int i = 0; i < n; i++) {
        event_t *e = &events[i];
        if (e->type == EV_ENTRY) {
            entries++;
            inside++;
            if (inside > peak) {
                peak = inside;
            }
        } else if (e->type == EV_EXIT) {
            exits++;
            if (inside > 0) {
                inside--;
            }
        } else if (e->type == EV_DENIED) {
            denied++;
        } else if (e->type == EV_ALERT) {
            alerts++;
        } else if (e->type == EV_LIGHT_ON && e->room < ROOM_COUNT) {
            light_on_since[e->room] = e->ts;
        } else if (e->type == EV_LIGHT_OFF && e->room < ROOM_COUNT) {
            if (light_on_since[e->room] != 0) {
                light_seconds[e->room] += (long)(e->ts - light_on_since[e->room]);
                light_on_since[e->room] = 0;
            }
        }
    }
    /* a light still on counts up to now */
    time_t now = time(NULL);
    for (int r = 0; r < ROOM_COUNT; r++) {
        if (light_on_since[r] != 0) {
            light_seconds[r] += (long)(now - light_on_since[r]);
        }
    }
    free(events);

    printf("--- Daily report (last %d events) ---\n", n);
    printf("Entries:        %d\n", entries);
    printf("Exits:          %d\n", exits);
    printf("Denied:         %d\n", denied);
    printf("Alerts:         %d\n", alerts);
    printf("Peak occupancy: %d\n", peak);
    printf("Inside now:     %d\n", core_people_inside());
    for (int r = 0; r < ROOM_COUNT; r++) {
        char dur[16];
        format_duration(light_seconds[r], dur, sizeof dur);
        printf("Room %d lights on: %s (h:mm:ss)\n", r + 1, dur);
    }
}

static void item_status(void)
{
    time_t now = time(NULL);
    char when[24];
    format_time(now, when, sizeof when);
    uint16_t start = 0;
    uint16_t end   = 0;
    core_schedule_get(&start, &end);

    printf("Time:        %s\n", when);
    printf("Schedule:    %02u:%02u - %02u:%02u (%s)\n", start / 60, start % 60, end / 60, end % 60,
           core_access_window_open(now) ? "OPEN" : "CLOSED");
    if (storage_sd_mounted()) {
        printf("SD card:     mounted, %s is %ld bytes\n", LOG_FILE_PATH, storage_log_bytes());
    } else {
        printf("SD card:     missing (RAM log)\n");
    }
    printf("Free heap:   %lu bytes\n", (unsigned long)esp_get_free_heap_size());
}

static void item_change_pin(void)
{
    if (!admin_login()) {
        return;
    }
    char pin1[INPUT_MAX];
    char pin2[INPUT_MAX];
    prompt("New PIN (4-8 digits): ", pin1, sizeof pin1);
    prompt("Repeat new PIN:       ", pin2, sizeof pin2);
    if (strcmp(pin1, pin2) != 0) {
        printf("PINs do not match.\n");
        return;
    }
    if (auth_set_pin(pin1) != ESP_OK) {
        printf("Rejected: use 4 to 8 digits.\n");
        return;
    }
    event_t e = { .ts = time(NULL), .type = EV_CONFIG, .uid = 0, .room = ROOM_COUNT,
                  .note = "PIN changed" };
    core_emit(&e);
    printf("PIN changed.\n");
}

/* ------------------------------------------------------------------ */
/* init + task                                                         */
/* ------------------------------------------------------------------ */

esp_err_t console_init(void)
{
    esp_err_t err = uart_driver_install(CONSOLE_UART, 256, 0, 0, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart driver install failed");
        return err;
    }
    uart_vfs_dev_use_driver(CONSOLE_UART);   /* printf now goes through the same driver */

    err = auth_init();
    if (err != ESP_OK) {
        return err;
    }

    BaseType_t ok = xTaskCreate(console_task, "console", 6144, NULL, 3, NULL);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "could not start task");
        return ESP_FAIL;
    }
    return ESP_OK;
}

static void console_task(void *arg)
{
    char choice[INPUT_MAX];

    vTaskDelay(pdMS_TO_TICKS(500));   /* let boot logs finish before the first menu */

    while (1) {
        show_menu();
        read_line(choice, sizeof choice);

        if (strcmp(choice, "1") == 0) {
            item_occupancy();
        } else if (strcmp(choice, "2") == 0) {
            item_who_inside();
        } else if (strcmp(choice, "3") == 0) {
            item_lights();
        } else if (strcmp(choice, "4") == 0) {
            item_access_log();
        } else if (strcmp(choice, "5") == 0) {
            item_set_schedule();
        } else if (strcmp(choice, "6") == 0) {
            item_set_time();
        } else if (strcmp(choice, "7") == 0) {
            item_daily_report();
        } else if (strcmp(choice, "8") == 0) {
            item_status();
        } else if (strcmp(choice, "9") == 0) {
            item_change_pin();
        } else if (strcmp(choice, "0") == 0) {
            printf("Bye. Press Enter to open the menu again.\n");
            read_line(choice, sizeof choice);
        } else {
            printf("Unknown option '%s'.\n", choice);
        }
    }
}
