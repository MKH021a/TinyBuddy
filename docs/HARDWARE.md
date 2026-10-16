# Hardware notes (milestone 1)

## Target

| Item | Value | Status |
|---|---|---|
| Board | ESP32-C3 Super Mini (ESP32-C3, single-core RISC-V, 160 MHz, **no FPU**, ~400 KB SRAM) | assumed from your brief, **no board was connected** during this session |
| Flash | Typically 4 MB embedded (ESP32-C3FH4). **Unverified**: run `memory` on the board (prints `flash chip` size) or `esptool.py flash_id` | unverified |
| App partition | PlatformIO `esp32-c3-devkitm-1` default table: 1,310,720-byte `app0` (two OTA slots). See TEST_REPORT.md for the measured image size | from the build output |
| Display | 1.3" 128x64 monochrome OLED, I2C | assumed |
| Controller | **Unknown**. 1.3" 128x64 modules are almost always **SH1106** (0.96" modules are usually SSD1306). Default build = SH1106 | unverified |

### Why SH1106 is the default for the physical board
The SH1106 has a 132-column RAM and no horizontal addressing mode; driving it as an SSD1306 shows the image shifted by 2 px with garbage columns at the right edge. 1.3" breakouts on the market overwhelmingly use the SH1106. Because this is a guess, the firmware lets you switch at runtime:

```
display ssd1306      # or: display sh1106   (applied immediately, saved to flash)
pattern              # 1px border must touch all four edges cleanly
probe                # I2C scan + status-byte heuristic (printed raw, labelled as a guess)
```

## Wiring (ESP32-C3 Super Mini → OLED)

| OLED pin | ESP32-C3 Super Mini | Notes |
|---|---|---|
| GND | GND | common ground |
| VCC | 3V3 | most modules accept 3.3-5 V; 3.3 V keeps I2C at 3.3 V logic |
| SCL | GPIO 9 | also the BOOT button (strapping pin; pulled high by the OLED's pull-ups = normal boot) |
| SDA | GPIO 8 | also the on-board blue LED (it may flicker with I2C traffic - harmless) |

GPIO 8/9 are the Arduino-ESP32 default I2C pins for the C3 and what Wokwi uses. To use other pins, change `TB_SDA` / `TB_SCL` in `firmware/platformio.ini` (or `firmware/src/Config.h`). Avoid GPIO 18/19 (USB D-/D+), GPIO 20/21 (UART0) and GPIO 2 (strapping) for I2C.

I2C address `0x3C` (some modules: `0x3D` → `TB_I2C_ADDR`). Bus clock 400 kHz by default; test faster clocks live with `i2c 800000` and watch `fps`.

## Wokwi vs. the real board
Wokwi has no "ESP32-C3 Super Mini"; the project uses `board-esp32-c3-devkitm-1` (same chip, pins `8`/`9`/`3V3.1`/`GND.1` verified against Wokwi's board definition) and the `board-ssd1306` part (Wokwi has no SH1106 part). The Wokwi build uses UART0 serial; the Super Mini builds use native USB CDC.
