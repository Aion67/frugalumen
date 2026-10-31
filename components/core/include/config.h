#pragma once
/*
 * config.h — the single source of truth for pins, addresses and timing.
 *
 * Never write a bare pin number inside a .c file. If a value might change,
 * it belongs here. Wiring in diagram.json must match this file.
 */
#include "driver/gpio.h"

/* ---- SPI bus: shared by both RFID readers and the microSD card ---- */
#define PIN_SPI_SCK          GPIO_NUM_18
#define PIN_SPI_MOSI         GPIO_NUM_23
#define PIN_SPI_MISO         GPIO_NUM_19
#define PIN_RFID_ENTRY_CS    GPIO_NUM_5    /* MFRC522 "SDA" pin on the entry reader */
#define PIN_RFID_EXIT_CS     GPIO_NUM_4    /* MFRC522 "SDA" pin on the exit reader  */
#define PIN_RFID_RST         GPIO_NUM_27   /* both readers share one reset line     */
#define PIN_SD_CS            GPIO_NUM_15

/* ---- I2C bus: LCD1602 backpack + DS1307 RTC ---- */
#define PIN_I2C_SDA          GPIO_NUM_21
#define PIN_I2C_SCL          GPIO_NUM_22
#define I2C_ADDR_LCD         0x27
#define I2C_ADDR_RTC         0x68
#define I2C_SPEED_HZ         100000

/* ---- Entrance ---- */
#define PIN_SERVO_ENTRY      GPIO_NUM_25
#define PIN_SERVO_EXIT       GPIO_NUM_26
#define PIN_BUZZER           GPIO_NUM_2

/* ---- Rooms ---- */
#define PIN_PIR_ROOM1        GPIO_NUM_32
#define PIN_LED_ROOM1        GPIO_NUM_14
#define PIN_PIR_ROOM2        GPIO_NUM_33
#define PIN_LED_ROOM2        GPIO_NUM_13

/* ---- Timing (milliseconds unless stated) ---- */
#define DOOR_OPEN_MS         3000   /* how long a door stays open after a valid scan   */
#define LIGHT_GRACE_MS       10000  /* motion must be absent this long before lights off */
#define DISPLAY_REFRESH_MS   1000
#define RFID_POLL_MS         150    /* how often each reader is asked "card present?"  */
#define RFID_REPEAT_MS       2000   /* same card on same reader within this = ignored  */
#define PIR_POLL_MS          100
#define OVERSTAY_CHECK_MS    1000

/* ---- Access policy ---- */
#define SCHEDULE_DEFAULT_START_MIN  (8 * 60)    /* 08:00 */
#define SCHEDULE_DEFAULT_END_MIN    (18 * 60)   /* 18:00 */
#define ADMIN_PIN_DEFAULT    "2468"             /* can be changed from the console, kept in NVS */
#define ADMIN_PIN_MAX_LEN    8
#define MAX_PEOPLE           64                 /* distinct tags the building remembers */
#define EVENT_QUEUE_LEN      32

/* ---- Storage ---- */
#define SD_MOUNT_POINT       "/sdcard"
#define LOG_FILE_PATH        SD_MOUNT_POINT "/events.csv"
#define RAM_LOG_LEN          64                 /* fallback ring buffer if the card is missing */
