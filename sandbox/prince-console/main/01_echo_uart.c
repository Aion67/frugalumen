/*
 * Lesson 01 — read characters from the serial port and echo them.
 *
 * UART0 is the port the serial monitor is attached to. ESP-IDF's log output
 * already goes there; to READ from it we install the UART driver, which runs
 * an interrupt that stores incoming bytes in a buffer for us.
 *
 * Type in the Wokwi serial monitor. Each character comes back with its code,
 * so you can see that Enter arrives as 13 ('\r'), sometimes followed by 10.
 */
#include <stdio.h>
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "lesson01";

void app_main(void)
{
    /* uart_driver_install(port, rx buffer bytes, tx buffer bytes (0 = write directly),
     *                     event queue size, queue handle, interrupt flags) */
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 256, 0, 0, NULL, 0));
    ESP_LOGI(TAG, "type something:");

    while (1) {
        uint8_t c;
        /* Wait up to 100 ms for one byte. Returns how many bytes were read (0 or 1). */
        int got = uart_read_bytes(UART_NUM_0, &c, 1, pdMS_TO_TICKS(100));
        if (got == 1) {
            printf("got '%c' (code %d)\n", (c >= 32 && c < 127) ? c : '?', c);
        }
        /* No explicit vTaskDelay needed: uart_read_bytes already blocks (yields). */
    }
}
