# display

**Owner:** Joel
**Brief section:** 2.5 (additional output interface)

## What it does
Refreshes a 16x2 LCD once a second from core: line 1 people inside and room lights,
line 2 clock plus OPEN / CLOSED / OVERSTAY n. Rewrites a line only when it changed.

## Pins used (see config.h)
I2C 21/22, PCF8574 backpack at 0x27.

## Public API
See `include/display.h`. Private: `lcd1602.c` (4-bit HD44780 over I2C).

## How it was tested
Sandbox: `sandbox/joel-display/`.

## Known limits
Read-only view; no buttons. If the backpack is at 0x3F, change `I2C_ADDR_LCD`.
