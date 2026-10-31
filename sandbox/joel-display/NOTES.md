# joel-display — notes

**What the module does.** Shows the building state on a 16x2 LCD and refreshes it once a
second: line 1 = people inside and which room lights are on, line 2 = clock and whether
access is OPEN, CLOSED or there is an OVERSTAY (window closed, people still inside). It only
reads core; it never changes anything.

**Pins.** I2C SDA 21, SCL 22. LCD backpack at 0x27 (some boards use 0x3F).

## Lessons (build one at a time — edit `LESSON` in `main/CMakeLists.txt`)

| File | Shows |
|---|---|
| `01_i2c_scan.c` | new I2C master driver, probe every address |
| `02_lcd_hello.c` | 4-bit HD44780 protocol over the PCF8574, spelled out |
| `03_full_module.c` | the real component reacting to core state |

## Gotchas I should be able to explain

- **Why two nibbles per byte:** the backpack only has 4 data lines free (the other 4 bits
  are RS, RW, E, backlight), so the LCD runs in 4-bit mode.
- **Why the "0x30 three times" start:** the LCD wakes in 8-bit mode; the datasheet reset
  sequence makes it listen reliably before we switch to 4-bit.
- **Why `esp_rom_delay_us` and not `vTaskDelay`:** the E pulse is 1 microsecond; one tick
  is 1 millisecond, far too coarse.
- **Why pad every line to 16 characters:** writing "In: 1" over "In: 12" would leave the
  "2" behind. Padding erases old text without a slow clear command.
- **Why rewrite only on change:** the display task runs every second; skipping unchanged
  lines keeps the I2C bus free for the RTC and avoids flicker.
- **Why the bus handle comes from `core_i2c_bus()`:** the RTC (in core) and the LCD share
  one bus. The new driver allows one `i2c_new_master_bus` per port, so one owner creates it.

## Tested how

Lesson 03 in Wokwi, watch the LCD change every 3 s. Final state shows "OVERSTAY 1".
