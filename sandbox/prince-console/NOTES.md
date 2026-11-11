# prince-console — notes

**What the module does.** The management interface on the serial port (requirement 2.5)
and the only place the access schedule can be changed (2.6). A numbered menu; items 5, 6
and 9 ask for the admin PIN first. The PIN and the schedule are stored in NVS, so both
survive a reset. The daily report replays the event log to compute entries, exits, denied,
peak occupancy and how long each room's light was on.

**Pins.** UART0 (TX 1, RX 3), the same port as the log output.

## Lessons (build one at a time — edit `LESSON` in `main/CMakeLists.txt`)

| File | Shows |
|---|---|
| `01_echo_uart.c` | install the UART driver, read one byte at a time |
| `02_read_line.c` | build a line: echo, backspace, CR/LF handling |
| `03_menu_and_pin.c` | if/else-if menu with a PIN gate |
| `04_full_module.c` | the real console over core + storage |

## Gotchas I should be able to explain

- **Why echo?** The Wokwi serial monitor (and most terminals) do not show what you type
  unless the device sends it back.
- **Why `fflush(stdout)` after a prompt:** without a newline, printf output can sit in a
  buffer and the prompt appears late or never.
- **Why Enter may arrive as `\r` then `\n`:** different monitors send different line
  endings; we accept either and ignore an empty line.
- **Why ask for the PIN every time instead of "logging in":** no session to forget to log
  out of; simplest thing that satisfies "only authenticated personnel".
- **Why the console reads through `storage_read_last()` instead of opening the file:** the
  file belongs to storage and is protected by its mutex. Two tasks on one `FILE` corrupts it.
- **Why `COMPONENTS main` in the root CMake:** our component is called `console`, same as
  one that ships with ESP-IDF. Limiting the build to what `main` needs avoids the clash.
- **Why the console is allowed `printf`:** the menu IS the serial output. Everywhere else,
  `ESP_LOGI` keeps logs tagged and filterable.

## Tested how

Lesson 04 in Wokwi: every menu item, wrong PIN refused, schedule persists across reset.
