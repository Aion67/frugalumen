# jovia-rfid_door — notes

**What the module does.** The entrance terminal. Two MFRC522 readers on the shared SPI
bus (entry on CS 5, exit on CS 4). One task polls them every 150 ms. For each tag it asks
core: known? access window open? anti-passback OK? A pass opens that lane's servo for 3 s
and beeps once; a refusal logs `EV_DENIED` with the reason and beeps twice.

**Pins.** SPI 18/23/19, CS 5 (entry) and 4 (exit), RST 27. Servos 25 (entry) and 26 (exit).
Buzzer 2.

## Lessons (build one at a time — edit `LESSON` in `main/CMakeLists.txt`)

| File | Shows |
|---|---|
| `01_spi_read_version.c` | SPI bus + device setup, read one register |
| `02_servo_sweep.c` | LEDC timer/channel, pulse width = angle |
| `03_buzzer_tones.c` | second LEDC timer, frequency = pitch |
| `04_read_card_uid.c` | REQA + anticollision through the FIFO |
| `05_full_module.c` | the real component with core deciding |

## Gotchas I should be able to explain

- **`.length` is in bits.** Two bytes = 16. Getting this wrong sends half a transaction.
- **Address byte format.** `(reg << 1)` with bit 7 set for read. The reply arrives in the
  SECOND byte; the first byte on MISO is junk clocked out while we sent the address.
- **REQA is a 7-bit frame.** `BitFramingReg = 0x07` tells the chip to send only 7 bits of
  the last byte. Forget it and no card ever answers.
- **Why one shared RST pin?** Both readers just need it high. Each has its own CS, which is
  how the SPI driver keeps their transactions apart (the bus is a shared resource, see
  `docs/architecture.md` §6).
- **Why the door timer is non-blocking.** If the task slept for 3 s with the door open, a
  scan on the other lane would wait 3 s. Instead we remember the tick to close at.
- **Why debounce by UID + time.** A card held on the reader answers every poll; we only
  act once per 2 s per card per reader.
- **Why core decides, not this module.** Entry/exit state is shared with the console and
  display; only core may change it, under its mutex.
- **4-byte UIDs only.** The 7-byte NFC tag preset needs cascade level 2; we treat its first
  4 bytes as the UID (it is "unknown" anyway).

## Tested how

Wokwi: tap presets on each reader, watch the serial events, the servo horn and listen for
the beeps. Red card is deliberately unknown.
