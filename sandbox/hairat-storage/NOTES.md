# hairat-storage — notes

**What the module does.** The only writer of the event log. One task blocks on core's event
queue; each event becomes one CSV line appended to `/sdcard/events.csv` and a copy in a
64-entry RAM ring. `storage_read_last()` and `storage_count()` read the file back for the
console (access log, daily report). If the card is missing the RAM ring is used, so the
rest of the system never notices.

**Pins.** SPI 18/23/19 shared with the RFID readers; card CS on 15.

## Lessons (build one at a time — edit `LESSON` in `main/CMakeLists.txt`)

| File | Shows |
|---|---|
| `01_mount_sd.c` | sdspi host + FATFS mount, print card info |
| `02_append_and_read.c` | fopen "a", fclose every line, fgets loop |
| `03_csv_events.c` | event_t <-> CSV line with snprintf / sscanf |
| `04_full_module.c` | the real component fed from the queue |

## Gotchas I should be able to explain

- **Why a queue between producers and the card:** the card is slow (milliseconds). The door
  task must not wait on it. Producers drop events in and leave; one consumer writes.
- **Why `fclose` after every line:** FATFS buffers writes; a reset before close loses them.
- **Why a second mutex (`s_file_lock`):** the logger task writes while the console task
  reads the same file. Two tasks touching one FILE at once corrupts it.
- **Why `max_freq_khz = 5000`:** the default 20 MHz is fine on hardware but conservative
  speeds are safer on a shared bus and in simulation.
- **Why the SPI bus is initialised in core, not here:** two modules use it. Whoever owns a
  shared resource must be the single place it is set up.
- **Wokwi free plan:** you cannot upload files to the simulated card, but the firmware can
  write to it, which is all we need. The card content is lost when the simulation stops.

## Tested how

Lesson 02: reset the board a few times, line count grows. Lesson 04: compare with and
without the card part in `diagram.json`.
