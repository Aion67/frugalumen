# storage

**Owner:** Hairat
**Brief section:** 2.4

## What it does
The single consumer of the event queue and the only writer of `/sdcard/events.csv`. Each
event is one CSV line; the file is closed after every line. A 64-entry RAM ring buffer
mirrors the newest events and takes over if the card is missing. Readers (console) go
through `storage_read_last()` / `storage_count()`, which share a mutex with the writer.

## Pins used (see config.h)
SPI 18/23/19 (shared), card CS 15.

## Public API
See `include/storage.h`.

## How it was tested
Sandbox: `sandbox/hairat-storage/`. RAM fallback verified in QEMU (no card).

## Known limits
Reading walks the whole file; fine for a day of events, slow for months. The daily report
looks at the newest 256 events.
