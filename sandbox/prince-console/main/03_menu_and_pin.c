/*
 * Lesson 03 — a tiny menu with one PIN-protected item.
 *
 * The menu is an if / else if chain on the typed string. Item 2 asks for a
 * PIN first and refuses without it. This is the whole console in miniature;
 * the real one has nine items and stores the PIN in NVS (see auth.c).
 */
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "driver/uart.h"
#include "driver/uart_vfs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "config.h"

static int s_secret = 42;

static void read_line(char *buf, size_t len)
{
    size_t n = 0;
    while (1) {
        uint8_t c;
        if (uart_read_bytes(UART_NUM_0, &c, 1, pdMS_TO_TICKS(100)) != 1) {
            continue;
        }
        if (c == '\r' || c == '\n') {
            if (n == 0) {
                continue;
            }
            break;
        }
        if ((c == 8 || c == 127) && n > 0) {
            n--;
            printf("\b \b");
            fflush(stdout);
            continue;
        }
        if (n < len - 1 && isprint(c)) {
            buf[n++] = (char)c;
            printf("%c", c);
            fflush(stdout);
        }
    }
    buf[n] = '\0';
    printf("\n");
}

static bool admin_login(void)
{
    char pin[16];
    printf("Admin PIN: ");
    fflush(stdout);
    read_line(pin, sizeof pin);
    if (strcmp(pin, ADMIN_PIN_DEFAULT) == 0) {
        return true;
    }
    printf("Wrong PIN.\n");
    return false;
}

void app_main(void)
{
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 256, 0, 0, NULL, 0));
    uart_vfs_dev_use_driver(UART_NUM_0);

    char choice[16];
    while (1) {
        printf("\n=== MINI MENU ===\n1. Show secret\n2. Change secret [admin]\n0. Nothing\nSelect: ");
        fflush(stdout);
        read_line(choice, sizeof choice);

        if (strcmp(choice, "1") == 0) {
            printf("secret = %d\n", s_secret);
        } else if (strcmp(choice, "2") == 0) {
            if (!admin_login()) {
                continue;              /* back to the menu */
            }
            char text[16];
            printf("New secret: ");
            fflush(stdout);
            read_line(text, sizeof text);
            s_secret = atoi(text);
            printf("saved.\n");
        } else if (strcmp(choice, "0") == 0) {
            printf("ok.\n");
        } else {
            printf("unknown option '%s'\n", choice);
        }
    }
}
