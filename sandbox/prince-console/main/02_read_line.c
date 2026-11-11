/*
 * Lesson 02 — collect characters into a line.
 *
 * A menu needs whole lines ("5", "09:00"), not single characters. Rules:
 *   - printable characters are stored and echoed (the monitor does not echo)
 *   - backspace (8 or 127) removes the last one
 *   - CR or LF ends the line; a LF right after a CR is ignored
 * printf without a newline sits in a buffer; fflush(stdout) pushes it out so
 * the prompt appears before we wait for input.
 */
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "driver/uart.h"
#include "driver/uart_vfs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "lesson02";

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
                continue;          /* empty line or the LF after a CR */
            }
            break;
        }
        if (c == 8 || c == 127) {
            if (n > 0) {
                n--;
                printf("\b \b");   /* move back, overwrite with space, move back */
                fflush(stdout);
            }
            continue;
        }
        if (n < len - 1 && isprint(c)) {
            buf[n] = (char)c;
            n++;
            printf("%c", c);       /* echo */
            fflush(stdout);
        }
    }
    buf[n] = '\0';
    printf("\n");
}

void app_main(void)
{
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 256, 0, 0, NULL, 0));
    uart_vfs_dev_use_driver(UART_NUM_0);   /* route printf through the driver too */

    char line[40];
    while (1) {
        printf("Say something: ");
        fflush(stdout);
        read_line(line, sizeof line);
        ESP_LOGI(TAG, "you typed '%s' (%d chars)", line, (int)strlen(line));
    }
}
