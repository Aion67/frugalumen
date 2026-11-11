/*
 * Lesson 01 — talk SPI to the MFRC522 and read its version register.
 *
 * SPI has four wires: SCK (clock), MOSI (master out), MISO (master in) and one
 * CS (chip select) per device. The master pulls CS low, clocks bytes out on
 * MOSI and, at the same time, bytes come back on MISO.
 *
 * The MFRC522 address byte is:  bit7 = 1 for read, bits 6..1 = register, bit0 = 0.
 * So to read register 0x37 we send (0x37 << 1) | 0x80 = 0xEE, then one dummy
 * byte while the chip answers. A real chip replies 0x91 or 0x92.
 */
#include <stdio.h>
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "config.h"

static const char *TAG = "lesson01";

#define REG_VERSION 0x37

void app_main(void)
{
    /* Hold the reader's reset pin high so it runs. */
    gpio_config_t rst = { .pin_bit_mask = 1ULL << PIN_RFID_RST, .mode = GPIO_MODE_OUTPUT };
    ESP_ERROR_CHECK(gpio_config(&rst));
    gpio_set_level(PIN_RFID_RST, 1);

    /* Step 1: the bus = which pins carry SCK/MOSI/MISO. One bus, many devices. */
    spi_bus_config_t bus = {
        .sclk_io_num   = PIN_SPI_SCK,
        .mosi_io_num   = PIN_SPI_MOSI,
        .miso_io_num   = PIN_SPI_MISO,
        .quadwp_io_num = -1,            /* -1 = not used (quad SPI pins) */
        .quadhd_io_num = -1,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));

    /* Step 2: the device = speed, mode and which CS pin. */
    spi_device_interface_config_t dev = {
        .clock_speed_hz = 1 * 1000 * 1000,
        .mode           = 0,            /* clock idle low, sample on rising edge */
        .spics_io_num   = PIN_RFID_ENTRY_CS,
        .queue_size     = 1,
    };
    spi_device_handle_t reader;
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &dev, &reader));

    while (1) {
        uint8_t tx[2] = { (REG_VERSION << 1) | 0x80, 0x00 };
        uint8_t rx[2] = { 0, 0 };
        spi_transaction_t t = {
            .length    = 16,            /* GOTCHA: length is in BITS. 2 bytes = 16 bits */
            .tx_buffer = tx,
            .rx_buffer = rx,
        };
        ESP_ERROR_CHECK(spi_device_transmit(reader, &t));

        /* rx[0] arrived while we sent the address (garbage); rx[1] is the answer. */
        ESP_LOGI(TAG, "VersionReg = 0x%02X %s", rx[1],
                 (rx[1] == 0x91 || rx[1] == 0x92) ? "(MFRC522 found)" : "(unexpected - check wiring)");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
