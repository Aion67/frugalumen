# core

**Owner:** Joshua
**Brief section:** 2.3, 2.7

## What it does
Holds the only copy of shared state (who is inside, room occupancy, access schedule) behind
one mutex. Owns the event queue every module writes to. Sets up the shared SPI and I2C
buses, reads the DS1307 into the system clock, saves the schedule in NVS, and runs a small
task that raises an `EV_ALERT` when the window closes with people inside.

## Pins used (see config.h)
SPI bus 18/23/19, I2C bus 21/22 (RTC at 0x68). No pins of its own.

## Public API
See `include/core.h`. Rule: take the mutex, copy or update, release, then do slow work.

## How it was tested
Sandbox: `sandbox/joshua-core/` (race demo, mutex fix, queue, NVS, RTC, full module).
Anti-passback refusals verified in QEMU and Wokwi.

## Known limits
Known tags are a fixed table in `core.c`. People table holds 64 distinct tags.
