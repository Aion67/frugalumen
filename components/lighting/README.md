# lighting

**Owner:** Shirley
**Brief section:** 2.2

## What it does
One task per room. PIR HIGH → LED on at once, `core_set_room_occupied(room, true)`,
`EV_LIGHT_ON`. No motion for `LIGHT_GRACE_MS` (10 s) → LED off, occupancy false,
`EV_LIGHT_OFF`. The daily report adds the on/off pairs into light-on time per room.

## Pins used (see config.h)
Room 1: PIR 32, LED 14. Room 2: PIR 33, LED 13.

## Public API
See `include/lighting.h` — `lighting_init(room, pir_pin, led_pin)` once per room.

## How it was tested
Sandbox: `sandbox/shirley-lighting/`. Wokwi PIR "Simulate Motion".

## Known limits
Light state equals occupancy state; there is no manual override.
