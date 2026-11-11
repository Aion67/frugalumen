# console

**Owner:** Prince
**Brief section:** 2.5, 2.6 (+ innovation: daily report)

## What it does
Numbered menu on UART0. Items: occupancy, who is inside, lights, last 10 log lines,
set schedule [admin], set time [admin], daily report, system status, change PIN [admin].
Admin items ask for the PIN every time; PIN and schedule live in NVS and survive reset.
Wrong PINs are logged as `EV_DENIED`.

## Pins used (see config.h)
UART0 (the serial monitor).

## Public API
See `include/console.h`. Private: `auth.c` (PIN in NVS).

## How it was tested
Sandbox: `sandbox/prince-console/`. Every item exercised in QEMU and Wokwi.

## Known limits
Single admin PIN, 4–8 digits, no lockout after repeated failures.
