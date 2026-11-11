# rfid_door

**Owner:** Jovia
**Brief section:** 2.1 (+ innovation: anti-passback, late-exit flag)

## What it does
One task polls two MFRC522 readers (entry, exit) every 150 ms. For each tag: known? window
open? anti-passback OK (via `core_record_entry/exit`)? A pass opens that lane's servo for
3 s and beeps once; a refusal logs `EV_DENIED` with the reason and beeps twice. Outside the
window entry is refused; exit is allowed only for people already inside and is flagged.

## Pins used (see config.h)
SPI 18/23/19; entry CS 5, exit CS 4, shared RST 27. Servos 25 (entry), 26 (exit). Buzzer 2.

## Public API
See `include/rfid_door.h`. Private drivers: `mfrc522.c`, `door_servo.c`, `buzzer.c`.

## How it was tested
Sandbox: `sandbox/jovia-rfid_door/`. Wokwi card presets, see `docs/wokwi.md`.

## Known limits
4-byte UIDs only (cascade level 1). Same card on the same reader within 2 s is ignored.
