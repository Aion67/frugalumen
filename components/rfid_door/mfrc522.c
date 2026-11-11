/*
 * mfrc522.c — talk to the MFRC522 RFID reader chip over SPI.
 *
 * The chip is a register file. Every SPI transaction is two bytes:
 *   byte 0 = address  (bit 7 set = read, bits 6..1 = register, bit 0 = 0)
 *   byte 1 = value to write, or a dummy 0x00 when reading
 *
 * To find a card we send two ISO 14443 commands through the chip's FIFO:
 *   REQA (0x26)          -> any idle card answers with 2 bytes (ATQA)
 *   ANTICOLL (0x93 0x20) -> the card answers with 4 UID bytes + 1 check byte
 */
#include <string.h>
#include "mfrc522.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "mfrc522";

/* registers we use (MFRC522 datasheet section 9) */
#define REG_COMMAND       0x01
#define REG_COM_IRQ       0x04
#define REG_ERROR         0x06
#define REG_FIFO_DATA     0x09
#define REG_FIFO_LEVEL    0x0A
#define REG_CONTROL       0x0C
#define REG_BIT_FRAMING   0x0D
#define REG_COLL          0x0E
#define REG_MODE          0x11
#define REG_TX_CONTROL    0x14
#define REG_TX_ASK        0x15
#define REG_T_MODE        0x2A
#define REG_T_PRESCALER   0x2B
#define REG_T_RELOAD_H    0x2C
#define REG_T_RELOAD_L    0x2D
#define REG_VERSION       0x37

/* commands written to REG_COMMAND */
#define CMD_IDLE          0x00
#define CMD_TRANSCEIVE    0x0C
#define CMD_SOFT_RESET    0x0F

/* card-side commands (ISO 14443-3) */
#define PICC_REQA         0x26
#define PICC_ANTICOLL_1   0x93

#define IRQ_RX            0x20
#define IRQ_IDLE          0x10
#define IRQ_TIMER         0x01
#define ERR_MASK          0x13   /* BufferOvfl | ParityErr | ProtocolErr */

#define TRANSCEIVE_TIMEOUT_MS 40
#define FIFO_READ_MAX         16

static esp_err_t write_reg(mfrc522_t *r, uint8_t reg, uint8_t value)
{
    uint8_t tx[2] = { (uint8_t)((reg << 1) & 0x7E), value };
    spi_transaction_t t = {
        .length    = 2 * 8,      /* in bits, not bytes */
        .tx_buffer = tx,
    };
    return spi_device_transmit(r->spi, &t);
}

static uint8_t read_reg(mfrc522_t *r, uint8_t reg)
{
    uint8_t tx[2] = { (uint8_t)(((reg << 1) & 0x7E) | 0x80), 0x00 };
    uint8_t rx[2] = { 0, 0 };
    spi_transaction_t t = {
        .length    = 2 * 8,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    spi_device_transmit(r->spi, &t);
    return rx[1];
}

/*
 * Read n bytes from the FIFO in ONE SPI transaction, the way the MFRC522
 * datasheet (section 8.1.2.1) describes: send the address, then keep sending
 * the address while clocking replies in, and send 0x00 for the final byte.
 * Reply byte i+1 is FIFO byte i; reply byte 0 is junk.
 */
static void read_fifo(mfrc522_t *r, uint8_t *out, int n)
{
    uint8_t tx[FIFO_READ_MAX + 1];
    uint8_t rx[FIFO_READ_MAX + 1];
    uint8_t addr = ((REG_FIFO_DATA << 1) & 0x7E) | 0x80;

    if (n > FIFO_READ_MAX) {
        n = FIFO_READ_MAX;
    }
    for (int i = 0; i < n; i++) {
        tx[i] = addr;
    }
    tx[n] = 0x00;

    spi_transaction_t t = {
        .length    = (n + 1) * 8,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    spi_device_transmit(r->spi, &t);
    for (int i = 0; i < n; i++) {
        out[i] = rx[i + 1];
    }
}

static void set_bits(mfrc522_t *r, uint8_t reg, uint8_t mask)
{
    write_reg(r, reg, read_reg(r, reg) | mask);
}

static void clear_bits(mfrc522_t *r, uint8_t reg, uint8_t mask)
{
    write_reg(r, reg, read_reg(r, reg) & (uint8_t)~mask);
}

esp_err_t mfrc522_init(mfrc522_t *reader, spi_host_device_t host, gpio_num_t cs_pin, const char *name)
{
    reader->name = name;

    spi_device_interface_config_t cfg = {
        .clock_speed_hz = 1 * 1000 * 1000,   /* 1 MHz is plenty and safe */
        .mode           = 0,                 /* MFRC522 is SPI mode 0 */
        .spics_io_num   = cs_pin,
        .queue_size     = 4,
    };
    esp_err_t err = spi_bus_add_device(host, &cfg, &reader->spi);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s: could not add SPI device", name);
        return err;
    }

    write_reg(reader, REG_COMMAND, CMD_SOFT_RESET);
    vTaskDelay(pdMS_TO_TICKS(50));

    /* Internal timer: fires ~25 ms after a transmit so a missing card does not hang us. */
    write_reg(reader, REG_T_MODE, 0x80);
    write_reg(reader, REG_T_PRESCALER, 0xA9);
    write_reg(reader, REG_T_RELOAD_H, 0x03);
    write_reg(reader, REG_T_RELOAD_L, 0xE8);

    write_reg(reader, REG_TX_ASK, 0x40);   /* 100% ASK modulation, required by ISO 14443A */
    write_reg(reader, REG_MODE, 0x3D);     /* CRC preset 0x6363 */
    set_bits(reader, REG_TX_CONTROL, 0x03);  /* antenna on */

    uint8_t version = read_reg(reader, REG_VERSION);
    ESP_LOGI(TAG, "%s reader ready, version 0x%02X", name, version);
    return ESP_OK;
}

/*
 * Send tx_len bytes to the card and collect the reply into rx.
 * bit_framing is 7 for the short REQA frame, 0 for whole bytes.
 * Returns the number of reply bytes, or 0 when nothing answered.
 */
static int transceive(mfrc522_t *r, const uint8_t *tx, int tx_len,
                      uint8_t *rx, int rx_max, uint8_t bit_framing)
{
    write_reg(r, REG_COMMAND, CMD_IDLE);
    write_reg(r, REG_COM_IRQ, 0x7F);          /* clear all interrupt flags */
    write_reg(r, REG_FIFO_LEVEL, 0x80);       /* flush FIFO */

    for (int i = 0; i < tx_len; i++) {
        write_reg(r, REG_FIFO_DATA, tx[i]);
    }
    write_reg(r, REG_BIT_FRAMING, bit_framing);
    write_reg(r, REG_COMMAND, CMD_TRANSCEIVE);
    set_bits(r, REG_BIT_FRAMING, 0x80);       /* StartSend */

    bool got_reply = false;
    for (int waited = 0; waited < TRANSCEIVE_TIMEOUT_MS; waited++) {
        uint8_t irq = read_reg(r, REG_COM_IRQ);
        if (irq & (IRQ_RX | IRQ_IDLE)) {
            got_reply = true;
            break;
        }
        if (irq & IRQ_TIMER) {
            break;                            /* chip's own timeout: no card */
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    clear_bits(r, REG_BIT_FRAMING, 0x80);

    if (!got_reply) {
        return 0;
    }
    if (read_reg(r, REG_ERROR) & ERR_MASK) {
        return 0;
    }

    int n = read_reg(r, REG_FIFO_LEVEL);
    if (n > rx_max) {
        n = rx_max;
    }
    read_fifo(r, rx, n);
    return n;
}

bool mfrc522_read_uid(mfrc522_t *reader, uint32_t *uid)
{
    uint8_t reply[8];

    /* Step 1: REQA is a 7-bit frame. Any idle card replies with 2 bytes. */
    clear_bits(reader, REG_COLL, 0x80);
    uint8_t reqa = PICC_REQA;
    int n = transceive(reader, &reqa, 1, reply, sizeof reply, 0x07);
    if (n != 2) {
        return false;
    }

    /* Step 2: anti-collision, cascade level 1. Reply = UID0..UID3 + BCC. */
    uint8_t anticoll[2] = { PICC_ANTICOLL_1, 0x20 };
    n = transceive(reader, anticoll, 2, reply, sizeof reply, 0x00);
    if (n != 5) {
        return false;
    }
    uint8_t bcc = reply[0] ^ reply[1] ^ reply[2] ^ reply[3];
    if (bcc != reply[4]) {
        ESP_LOGW(TAG, "%s: UID checksum mismatch, got %02X %02X %02X %02X bcc %02X",
                 reader->name, reply[0], reply[1], reply[2], reply[3], reply[4]);
        return false;
    }

    *uid = ((uint32_t)reply[0] << 24) | ((uint32_t)reply[1] << 16) |
           ((uint32_t)reply[2] << 8)  |  (uint32_t)reply[3];
    return true;
}
