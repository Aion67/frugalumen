#pragma once
/*
 * Private to rfid_door: a minimal MFRC522 driver over SPI.
 * It does just enough to answer "is a card here, and what is its 4-byte UID?".
 */
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

typedef struct {
    spi_device_handle_t spi;
    const char         *name;   /* "entry" / "exit", for log lines */
} mfrc522_t;

esp_err_t mfrc522_init(mfrc522_t *reader, spi_host_device_t host, gpio_num_t cs_pin, const char *name);

/* Returns true and fills *uid when a card is in the field right now. */
bool mfrc522_read_uid(mfrc522_t *reader, uint32_t *uid);
