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
