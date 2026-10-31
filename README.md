# frugalumen

Smart Control & Monitoring System for CoCIS, Makerere University — access control at the
main entrance, automatic lighting in two lecture rooms, logging to microSD, a serial
management console and an LCD status board. **One ESP32, ESP-IDF v6, simulated in Wokwi.**

> This branch is a reference implementation for the group to learn from and compare
> against. See `CLAUDE.md` for the ground rules and `docs/` for the brief, architecture,
> coding style and the sandbox idea.

## Quick start

```bash
idf.py set-target esp32
idf.py build
idf.py uf2          # -> build/uf2.bin, upload it in Wokwi
```

Full run instructions, demo script and card list: **[docs/wokwi.md](docs/wokwi.md)**.

## What is where

| Path | What |
|---|---|
| `main/main.c` | `app_main`: calls each module's `init()` and nothing else |
| `components/core/` | shared state behind one mutex, event queue, buses, RTC, NVS schedule |
| `components/rfid_door/` | entrance: two MFRC522 readers, two servo doors, buzzer |
| `components/lighting/` | one task per room: PIR → LED with a grace timer |
| `components/storage/` | the single SD writer; CSV event log with RAM fallback |
| `components/console/` | numbered serial menu, PIN-protected admin items, daily report |
| `components/display/` | LCD1602 over I2C, refreshed once a second |
| `components/core/include/config.h` | **every pin, address and timeout** |
| `sandbox/<owner>-<module>/` | numbered, heavily commented lessons building up to each module |
| `diagram.json` | Wokwi wiring, matches `config.h` |

## How the requirements map

| Brief | Where |
|---|---|
| 2.1 Access control, entry/exit records | `rfid_door` decides, `core_record_entry/exit` keep state, `storage` logs |
| 2.2 Automatic lighting | `lighting` |
| 2.3 Three terminals | three FreeRTOS tasks: `rfid_door`, `light1`, `light2` |
| 2.4 microSD storage | `storage` (`/sdcard/events.csv`) |
| 2.5 Serial menus + LCD | `console`, `display` |
| 2.6 Configurable access window, admin only | console item 5 behind the PIN; saved in NVS |
| 2.7 Synchronisation | mutex in `core`, FreeRTOS queue for events, mutex on the log file, SPI driver per-device locking |
| Innovation | anti-passback, overstay alert with flagged late exits, daily occupancy/energy report |

## Hardware (all in `config.h`)

SPI 18/23/19 shared by entry reader (CS 5), exit reader (CS 4) and SD (CS 15). I2C 21/22
for the LCD (0x27) and DS1307 (0x68). Servos 25/26, buzzer 2. Room 1 PIR 32 / LED 14,
Room 2 PIR 33 / LED 13.
