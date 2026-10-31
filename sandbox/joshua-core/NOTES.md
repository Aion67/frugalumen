# joshua-core — notes

**What the module does.** Holds the one true copy of: who is inside, which rooms are
occupied, the access schedule. Every other module reads or changes these only through
`core_*()` functions. It also owns the event queue, the SPI/I2C buses, the RTC and the
NVS-saved schedule, because each of those is shared by more than one module.

**Pins.** None of its own. It sets up the buses: SPI (18/23/19), I2C (21/22). The RTC is
on I2C at 0x68.

## Lessons (build one at a time — edit `LESSON` in `main/CMakeLists.txt`)

| File | Shows |
|---|---|
| `01_shared_counter_race.c` | lost updates when two tasks share a variable |
| `02_mutex_fix.c` | the same code fixed with `xSemaphoreTake/Give` |
| `03_queue_producer_consumer.c` | many producers, one consumer, nobody waits for the slow one |
| `04_nvs_remember.c` | a boot counter that survives reset |
| `05_rtc_clock.c` | reading the DS1307 with the new I2C driver |
| `06_full_core.c` | the real `core` component: anti-passback, schedule, events |

## Gotchas I should be able to explain

- **Why `counter++` is not safe:** it is read-add-write, three steps; another task can
  slip in between.
- **Why take-copy-release:** holding the mutex while printing or writing the SD card
  makes every other task wait. Core copies state out and releases before slow work.
- **Why the queue uses timeout 0 in `core_emit`:** the door task must never block because
  the SD card is busy. Dropping one log line beats freezing the door.
- **Why `ESP_ERR_NVS_NOT_FOUND` on first boot is fine:** the key simply does not exist yet.
- **Why `xTaskCreate` is not wrapped in `ESP_ERROR_CHECK`:** it returns `pdPASS` (1), not
  `ESP_OK` (0). Wrapping it would panic on success.
- **Why `volatile`:** the compiler may otherwise cache a variable in a register and never
  notice another task/ISR changed it.

## Tested how

Lessons 01–04 and 06 run with just the ESP32 in Wokwi. Lesson 05 needs the DS1307 wired to
21/22 (see `diagram.json` here).

## Still unclear / to ask

- What should happen if the people table (64) fills up? Currently refuses entry.
