# Test report — milestone 1

Date: 2026-10-16. Host: macOS (Apple clang 17), PlatformIO 6.2.0, espressif32 6.13.0
(Arduino-ESP32 **2.0.17**, riscv32-esp GCC 8.4.0), U8g2 2.36.2.

**What kind of test each line is** — `SIM` = run in the native simulator (same C++ core as the
firmware, compiled for the Mac); `BUILD` = compile/link only; `DEVICE` = on the real board;
`UNVERIFIED` = not run.

## Summary

| Area | Result | Kind |
|---|---|---|
| Core QA suite (`make test`) | **45 / 45 pass** (full output: `docs/qa_output.txt`) | SIM |
| Firmware `sh1106` / `ssd1306` / `wokwi` builds | success, 0 warnings from project code with `-Wall` | BUILD |
| Generated Arduino sketch + wokwi.com bundle | both compile (identical sizes to the PlatformIO builds) | BUILD |
| Running on the ESP32-C3 Super Mini | **not run — no board was connected** (no USB serial device found) | UNVERIFIED |
| Real OLED rendering, SH1106 vs SSD1306 | **not run** | UNVERIFIED |
| Wokwi simulation | **not run** (Wokwi needs your browser session or a `WOKWI_CLI_TOKEN`) | UNVERIFIED |
| On-device frame rate, render time, heap at runtime | **not measured** — the `fps` and `memory` commands report them on hardware | UNVERIFIED |

## Core QA suite (SIM)

`./build/tbsim test` renders every animation × every face × both mirror variants at 60 fps and checks:

| Check | Result |
|---|---|
| Catalog integrity: unique ids/names, keys time-ordered, every key/effect ends inside the duration, lookup by id and by name | pass |
| #80 on faces A–F, normal + mirrored (12 runs): **final frame is pixel-identical to the resting face and no effect remains** | pass (0 px differ, 0 effects) |
| Eyes never leave the screen horizontally; no lit pixel on the screen border (clipping) | pass (0 border px) |
| Stray pixels: max isolated lit pixels per frame (effects excluded) ≤ 2 | pass (max 1 — single-pixel catch-lights) |
| Determinism: same seed → identical 20 s idle+play reel (frame hash) | pass |
| Variation: different seed → different reel | pass |
| `millis()` rollover: a reel that crosses the 32-bit wrap 4 s in is frame-identical to one starting at 0 | pass |
| Interruption: `stop` during blink / look / POP (hearts on screen) / bounce / wink → effects cleared immediately, idle restored within 400 ms | pass |
| Autonomous mode: plays on its own with idle gaps (90 s) | pass (5 plays) |
| Style-parameter reflection: set/get floats, enums, groups; rejects bad keys/values | pass |
| Host render cost (informational) | 8.2 µs/frame on this Mac — **not** a device number |

Defects this suite (and frame-stepping in the previewer) caught and that were fixed before this report:
tall eyes leaving the top edge on the big leap (A/B/C/F), a one-row starlight dither creating 28
stray pixels on F mid-blink, solid white "domes" flashing during the arch transition, an iris
squeezed into an "outlined box" while winking, a 1 px kink in the closed-eye seam, and hearts
clipped by the top edge.

## Firmware build (BUILD)

| Environment | Flash used* | App partition | Flash % | Static RAM (data+bss) | `firmware.bin` |
|---|---|---|---|---|---|
| `sh1106` (Super Mini, USB CDC) | 336,880 B | 1,310,720 B | 25.7 % | 18,816 B of 327,680 B (5.7 %) | 356,480 B |
| `ssd1306` (Super Mini, USB CDC) | 336,880 B | 1,310,720 B | 25.7 % | 18,816 B (5.7 %) | — |
| `wokwi` (UART serial) | 351,244 B | 1,310,720 B | 26.8 % | 18,920 B (5.8 %) | — |

\* PlatformIO's figure (text + rodata + data). Section sizes for `sh1106`: `.flash.text` 219,958 B,
`.flash.rodata` 60,040 B, `.iram0.text` 49,250 B, `.dram0.data` 7,632 B, `.dram0.bss` 11,184 B.
Partition table: Arduino `default.csv` (two 0x140000 OTA app slots + 0x160000 SPIFFS) on the
board definition's 4 MB flash. **The real chip's flash size is unverified** (run `memory`).

The 70–80 % flash target is a milestone-2 topic: today's image is mostly the Arduino/ESP-IDF
runtime; the face core is small and procedural. Filling flash is only worthwhile with meaningful
assets (the full 80-animation catalog, optional bitmap effects), never padding.

## How to reproduce

```bash
make test                                   # core QA suite
cd firmware && pio run -e sh1106 -e ssd1306 -e wokwi
```

On the board (after flashing): `test all`, `fps`, `memory`, `probe`, `pattern`.
