# shirley-lighting — notes

**What the module does.** One task per lecture room. Reads the PIR every 100 ms. Motion
turns the LED on at once and tells core the room is occupied. When there has been no motion
for `LIGHT_GRACE_MS` (10 s) the LED goes off and core is told the room is empty. Every
on/off is logged as an event (`EV_LIGHT_ON` / `EV_LIGHT_OFF`), which the daily report
adds up into "hours lights were on".

**Pins.** Room 1: PIR 32, LED 14. Room 2: PIR 33, LED 13. (All in `config.h`.)

## Lessons (build one at a time — edit `LESSON` in `main/CMakeLists.txt`)

| File | Shows |
|---|---|
| `01_read_pir.c` | configure an input and read it |
| `02_led_on_off.c` | configure an output and toggle it |
| `03_motion_light.c` | LED = PIR, and why that flickers |
| `04_grace_timer.c` | remember last motion, switch off after a delay |
| `05_full_module.c` | the real component, two rooms, events to core |

## Gotchas I should be able to explain

- **Why poll instead of an interrupt?** Polling every 100 ms is simple, costs nothing
  noticeable, and avoids ISR rules (IRAM_ATTR, no logging, volatile flags). An interrupt
  would only matter if we needed microsecond reaction.
- **Why `pull_down_en`?** So the pin reads a clean 0 when the sensor is idle instead of
  floating.
- **Why ticks and not `time(NULL)`?** The grace period is a short duration; the tick
  counter is monotonic and does not jump when the admin sets the clock.
- **Why one struct per room?** Two tasks running the same function must not share
  variables, or room 1's timer would reset room 2's light.
- **Why does the Wokwi PIR go LOW after 5 s?** Its `delayTime` attribute. The grace timer is
  what keeps the light on between movements.

## Tested how

Wokwi: click a PIR → "Simulate Motion". LED lights at once; goes off 10 s after the PIR
output drops. Serial shows the events.
