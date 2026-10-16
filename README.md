# TinyBuddy — milestone 1: face lab + first animation

TinyBuddy is a tiny animated companion for an **ESP32-C3 Super Mini** with a **1.3" 128×64
monochrome OLED**. This milestone delivers the design pipeline and one fully choreographed
animation, so you can choose the face before the full 80-animation catalog is built.

| Delivered now | Status |
|---|---|
| Shared C++ face renderer + animation engine (`core/`) — one codebase for desktop preview *and* firmware | done |
| Six face candidates, 6 documented design rounds (`previews/round_1` … `round_6`) | done, **awaiting your choice** |
| Local previewer: web face lab + CLI contact sheets, GIFs, key poses, filmstrips | done |
| Animation **#80 TinyBuddy Signature Finale** + natural idle behaviour | done |
| ESP32-C3 test firmware (all six faces, #80, idle, serial console, SH1106/SSD1306 switch) | builds; **not yet run on hardware** |
| Wokwi project | files ready; **not run** |
| Animations #01–#79, personality system, flash-use target | milestone 2, after approval |

See `docs/TEST_REPORT.md` for exactly what was tested, how, and what is still unverified.

---

## 1. See the faces (2 minutes)

You need a C++17 compiler (macOS: `xcode-select --install`; Linux: `sudo apt install g++ make`) and Python 3.

```bash
cd TinyBuddy
python3 preview/server.py
```

Open **http://127.0.0.1:8765**. The server compiles `build/tbsim` (the firmware's renderer,
built for your computer) and every pixel you see comes from it.

- **Faces** — all six candidates, any of 13 expressions, true 1× pixels, panel tint. Mark each
  👍 / 🤔 / 👎 with notes, or **★ Make canonical** to freeze one as the reference
  (`reference/approved_face.json` + renders). Reviews are saved to `previews/reviews.json`.
- **Animate** — play #80 on any face, frame-step (← →), scrub the phase timeline, ¼×–2× speed,
  mirrored variant, **all six in sync**, **idle + auto reel**, PNG export, approximate true size.
- **Tune** — sliders for every style parameter, live; copy the overrides or approve with them.
- **Design rounds** — every round's contact sheet and my inspection notes.

### CLI exports (needs Pillow: `python3 -m pip install pillow`)

```bash
python3 preview/tbpreview.py round 7 --note "my tweak"   # new round folder: sheet, PNGs, .bin, notes source snapshot
python3 preview/tbpreview.py anim 80 ABCDEF              # key poses, filmstrips, GIFs, all-faces GIF
python3 preview/tbpreview.py compare F 1 2 3 4 5 6       # one face across rounds
python3 preview/tbpreview.py lab test --face F --var "big iris|pupilRx=14.5 pupilRy=16"
make test                                                # automated QA suite (45 checks)
```

Ready-made outputs: `previews/round_6/contact_sheet.png`, `previews/animations/anim_80/`
(`anim_80_all_faces.gif`, `keyposes_*.png`, `filmstrip_*.png`).

## 2. The six candidates

| | Name | Idea |
|---|---|---|
| A | Mochi | soft round eyes, big glossy iris, dot catch-lights, cat mouth, hatch blush |
| B | Kira | tiny anime companion: dithered "starlight" lower iris, thin winged lash |
| C | Pebble | minimal premium: solid rounded slabs; gaze moves the whole eye; mouth only for big emotions |
| D | Imp | mischievous: solid sly half-lids with a tilt, smiling-arch bottoms, cat smirk, side glance |
| E | Dozy | sleepy plush: heavy curved lid, big low iris, tiny "o" mouth, dithered cheeks |
| F | TinyBuddy | signature blend: A's glossy proportions + B's starlight glow + ✦ sparkle catch-light |

Any of them can be combined: tell me "F's eyes with A's dots" or paste overrides from **Tune**.

## 3. Hardware and wiring

| OLED | ESP32-C3 Super Mini |
|---|---|
| GND | GND |
| VCC | 3V3 |
| SCL | GPIO 9 |
| SDA | GPIO 8 |

I2C address `0x3C`, 400 kHz. 1.3" modules are almost always **SH1106** (the default build). If the
picture looks shifted or has a noisy right edge, type `display ssd1306` in the serial monitor
(saved; no rebuild). Details, pin caveats and Wokwi differences: `docs/HARDWARE.md`.

## 4. Build and flash the firmware

Install PlatformIO once: `python3 -m pip install platformio` (or the VS Code PlatformIO extension).

```bash
cd TinyBuddy/firmware
pio run -e sh1106 -t upload      # 1.3" SH1106 module (use -e ssd1306 for SSD1306)
pio device monitor               # 115200 baud, then type: help
```

If upload fails on a Super Mini: hold **BOOT**, tap **RESET**, release BOOT, upload again.

**Arduino IDE instead:** `python3 tools/make_arduino_sketch.py`, open
`firmware/arduino/TinyBuddy/TinyBuddy.ino`, board *ESP32C3 Dev Module*, *USB CDC On Boot: Enabled*,
library *U8g2*. (Generated from the same sources — never edit the generated folders.)

## 5. Wokwi simulation

- **VS Code + Wokwi extension:** `cd firmware && pio run -e wokwi`, then *Wokwi: Start Simulator*
  (uses `firmware/wokwi.toml` + `firmware/diagram.json`).
- **wokwi.com:** create an *ESP32-C3* Arduino project, run `python3 tools/make_arduino_sketch.py`,
  and copy every file from `firmware/arduino/wokwi_web/` (incl. `diagram.json`, `libraries.txt`) into it.

Then in the serial monitor: `help`, `list`, `play 80`, `faces`, `face A`, `test all`, `auto on`.
If the screen stays black: check `probe` output and that `libraries.txt` contains `U8g2`.
Wokwi simulates an ESP32-C3-DevKitM-1 with an SSD1306, not the exact Super Mini/SH1106.

## 6. Serial commands (milestone 1)

| Command | Does |
|---|---|
| `help`, `list`, `faces` | commands, animations, design candidates |
| `play 80` / `play finale` / `play 80 m` | play by id / name / mirrored |
| `random`, `idle`, `stop` | random pick, back to idle, safe interruption |
| `auto on` / `auto off` | autonomous vs manual mode (saved) |
| `face A`…`face F` | switch candidate live on the real OLED (saved) |
| `set pupilRx 14`, `get eyeW` | live style tweaks on the device |
| `status`, `fps`, `memory` | mode/uptime, measured cadence + render/transfer µs, heap/flash |
| `display sh1106` / `display ssd1306`, `probe`, `pattern`, `contrast 0-255`, `i2c 800000` | display diagnostics |
| `seed 42`, `test all`, `reboot` | reproducible behaviour, playback self-test |

The personality commands from the spec arrive with the full catalog in milestone 2.

## 7. How it works

- **Expression** = a static pose (`tb_anim.cpp`, 13 of them). **Motion** = a tween on one channel.
  **Animation** = a timed table of keys, oscillators and effect events (`tb_catalog.cpp`).
- `evaluate(animation, t, variant, basePose)` is a pure function of time, so playback is
  non-blocking, frame-steppable, rollover-safe and reproducible.
- `FaceState` has 37 channels (lids, cheeks, tilt, scale, pupils, gaze, brows, mouth family, blush,
  squash). Keys can be absolute or relative to the resting pose, so one choreography fits all faces.
- The renderer rasterises each eye per column into vertical spans (cheap in OLED page layout);
  per-pixel work is integer, so it suits the C3's FPU-less core. It draws straight into U8g2's
  1024-byte buffer: one framebuffer, one I2C transfer per frame.

**Add an expression:** add a `ChVal` array + one line to `kExpressions` in `core/src/tb_anim.cpp`.
**Add an animation:** add a `Key[]` (+ optional `FxEv[]`, `Marker[]`) and one `AnimDef` line in
`core/src/tb_catalog.cpp`; run `make test` (it checks ordering, endings and stray pixels for every
face) and `python3 preview/tbpreview.py anim <id>`.

## 8. Layout

```
core/          shared engine (tb_*.h/.cpp) + library.json
sim/           native simulator, exporters, QA suite (tbsim)
preview/       server.py + web/ (face lab), tbpreview.py (CLI sheets, GIFs)
previews/      round_1..6, lab/, animations/anim_80/, compare_*.png
reference/     filled when you approve a face
firmware/      platformio.ini, src/, diagram.json, wokwi.toml, arduino/ (generated)
docs/          HARDWARE.md, ANIMATION_CATALOG.md, TEST_REPORT.md, qa_output.txt
tools/         make_arduino_sketch.py
```

## 9. Known limitations

- Nothing has run on a real ESP32-C3 or OLED yet; on-device fps/heap are unmeasured.
- The OLED controller (SH1106 vs SSD1306) and the board's flash size are unverified guesses.
- The `probe` controller guess is a heuristic; `pattern` is the reliable check.
- The previewer's OLED glow is cosmetic; pixel positions are exact, brightness and bloom are not.
- One animation of 80 is implemented by design (approval gate).
