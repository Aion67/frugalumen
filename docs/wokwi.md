# Running the project in Wokwi (free plan)

Wokwi's **browser simulator is free** for public projects. The VS Code extension is a paid
add-on (30-day trial available), so the steps below use the browser. Everything is built
locally with ESP-IDF; Wokwi only runs the finished firmware.

## 0. Prerequisites

- ESP-IDF **v6.0 or newer** installed and exported (`idf.py --version` works). The project
  uses the new I2C driver and `esp_driver_*` components, which older releases do not have.
- A free account at <https://wokwi.com>.

## 1. Build

```bash
cd frugalumen
idf.py set-target esp32      # first time only
idf.py build
idf.py uf2                   # packs bootloader + partitions + app into build/uf2.bin
```

`idf.py uf2` is the file Wokwi wants. (`idf.py merge-bin` also works; upload
`build/merged-binary.bin` instead.)

## 2. Create the Wokwi project (once)

1. Go to <https://wokwi.com>, sign in, click **+ New Project** and choose **ESP32**
   (any ESP32 template is fine; we replace its firmware).
2. Open the **diagram.json** tab, select everything and paste the contents of this repo's
   `diagram.json`. The parts and wiring match `components/core/include/config.h`.
3. Click **Save**. Free projects are public; that is expected.

## 3. Upload the firmware and run

1. In the Wokwi editor press **F1** and choose **"Upload Firmware and Start Simulation…"**.
2. Select `build/uf2.bin`.
3. The simulation starts. The serial monitor shows the boot log and then the menu:

```
=== CoCIS CONTROL ===
1. Show occupancy
2. Who is inside
3. Show lights
4. View access log
5. Set schedule       [admin]
6. Set time           [admin]
7. Daily report
8. System status
9. Change admin PIN   [admin]
0. Exit
Select:
```

Type a number in the serial monitor input box and press Enter. Admin PIN is **2468**.

After every `idf.py build && idf.py uf2`, repeat step 3 to load the new firmware.

## 4. Demo script

The access window defaults to **08:00–18:00** and the clock comes from the DS1307, which
Wokwi sets to your PC's time. If you demo outside those hours, first use menu **5** to widen
the window (or **6** to set the time).

| Do this in Wokwi | Expect |
|---|---|
| Click the **entry** reader → pick **Blue Card** → **Tap** | Serial: `ENTRY Joan Nakato`, entry servo turns for 3 s, one beep, LCD `In: 1` |
| Wait 2 s, tap Blue on the entry reader **again** | `DENIED … already inside`, two low beeps (anti-passback) |
| Tap **Red Card** anywhere | `DENIED … unknown tag` (red is deliberately not enrolled) |
| Tap Blue on the **exit** reader | `EXIT Joan Nakato`, exit servo turns, LCD `In: 0` |
| Tap Green on the **exit** reader without entering | `DENIED … not inside` |
| Click a **PIR** → **Simulate Motion** | Room LED on at once, `L1:*` on the LCD; off ~10 s after motion stops |
| Menu **4** | last 10 events read back from the SD card |
| Menu **7** | entries, exits, denied, peak occupancy, light-on time per room |
| Menu **5**, PIN 2468, set a window that ends in a minute, enter with Blue, wait | `ALERT overstay:1` in the log, LCD shows `OVERSTAY 1`; Blue can still exit (flagged `late exit`) |
| Menu **5** then press the board's **reset** button, menu **8** | schedule survived the reset (NVS) |

Card presets and who they are (`components/core/core.c`):

| Wokwi preset | UID | Person |
|---|---|---|
| Blue Card | 01:02:03:04 | Joan Nakato (student) |
| Green Card | 11:22:33:44 | Peter Okello (student) |
| Yellow Card | 55:66:77:88 | Dr. Nabukenya (lecturer) |
| Key Fob | C0:FF:EE:99 | Night guard |
| Red Card, NFC Tag | — | not enrolled → denied |

## 5. Sandboxes

Each `sandbox/<owner>-<module>/` is its own ESP-IDF project with numbered lesson files.
Pick the lesson in `main/CMakeLists.txt` (the `set(LESSON …)` line), then:

```bash
cd sandbox/shirley-lighting
idf.py build && idf.py uf2
```

and upload `build/uf2.bin` to a Wokwi project whose `diagram.json` is the one in that
sandbox folder (it only contains the parts that lesson needs).

## 6. Alternatives

- **Wokwi for VS Code** (paid / trial): `wokwi.toml` files are already in place. Build,
  then **F1 → Wokwi: Start Simulator**. It reloads automatically after each build.
- **Wokwi CLI**: `wokwi-cli --interactive .` with a `WOKWI_CLI_TOKEN`. Needs CI minutes.

## 7. Where are the SD card files?

**Browser:** nowhere you can click. The free simulator keeps the card in memory and throws
it away when the simulation stops; the "SD Card" tab only lists files uploaded beforehand
(a paid feature). The log is still real:

- menu **8** prints the size of `/sdcard/events.csv`, growing with every event
- menu **4** and **7** read their rows back from that file
- press the board's **RST** button: the firmware restarts, the card does not, and the
  counts in **7** carry on from where they were

**VS Code / wokwi-cli:** `wokwi.toml` has an `[[sdcard]]` section with `writeback = true`.
When the simulation stops, the card is copied into `sdcard/` and `events.csv` can be
opened on the PC.

## 8. Known simulation limits

- The simulated CPU runs slower than real silicon; polling intervals in `config.h` are
  generous on purpose.
- Free plan: you cannot upload files to the virtual SD card, but the firmware writes to it
  normally. Its contents vanish when the simulation stops.
- The 7-byte "NFC Tag" preset is read as a 4-byte UID (cascade level 1 only); it is not
  enrolled anyway.
- If the LCD stays blank, try `i2cAddress: "0x3F"` on the part **and** `I2C_ADDR_LCD` in
  `config.h`; they must match.
