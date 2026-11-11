**Wokwi parts for the system with their protocols:**

| Part | Wokwi name | Protocol |
|---|---|---|
| RFID reader | MFRC522 | **SPI** |
| SD card module | microSD card | **SPI** |
| RTC | DS1307 | **I2C** |
| LCD 16×2 | LCD1602 (I2C backpack) | **I2C** |
| PIR motion sensor | PIR motion sensor | **GPIO** (1 pin, HIGH on motion) |
| Servo (door) | Servo | **PWM** (1 pin) |
| LED (lights) | LED | **GPIO** (or PWM to dim) |
| Buzzer | Buzzer | **GPIO** (or PWM for tones) |
| Pushbutton | Pushbutton | **GPIO** |
| Serial console | built-in | **UART** |


Peripherals
- RFID reader (MFRC522, SPI)
- SD card module (microSD card, SPI)
- RTC (DS1307, I2C)
- LCD 16×2 (LCD1602 with I2C backpack, I2C)
- PIR motion sensor (PIR motion sensor, GPIO — 1 pin, HIGH on motion)
- Servo / Door (Servo, PWM — 1 pin)
- LED / Lights (LED, GPIO or PWM for dimming)
- Buzzer (Buzzer, GPIO or PWM for tones)
- Pushbutton (Pushbutton, GPIO)
- Serial console (Built-in, UART)
