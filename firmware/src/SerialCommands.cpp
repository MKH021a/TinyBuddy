#include "SerialCommands.h"

#include <Arduino.h>
#include <esp_system.h>

#include "App.h"
#include "tb_catalog.h"
#include "tb_params.h"

using namespace tb;

namespace {

bool is(const char* a, const char* b) { return a && strcasecmp(a, b) == 0; }

// Parses a non-negative integer; returns false on junk ("12x", "", "-3").
bool parseUInt(const char* s, long& out, long lo, long hi) {
  if (!s || !*s) return false;
  char* end = nullptr;
  long v = strtol(s, &end, 0);
  if (*end != '\0' || v < lo || v > hi) return false;
  out = v;
  return true;
}

void listAnimations() {
  Serial.printf("%d animation(s) implemented (milestone 1; 80 planned after face approval)\n", animCount());
  for (int i = 0; i < animCount(); ++i) {
    const AnimDef& a = animAt(i);
    Serial.printf("  #%02d  %-30s  %-18s %5u ms\n", a.id, a.name, a.category, a.duration);
  }
}

void listFaces() {
  for (int i = 0; i < styleCount(); ++i) {
    const FaceStyle& s = styleAt(i);
    Serial.printf("  %c  %-10s %s%s\n", s.id, s.name, s.motto, s.id == app.styleId ? "   <- current" : "");
  }
}

void status(uint32_t now) {
  const AnimDef* a = app.buddy.current();
  uint32_t up = now / 1000;
  Serial.printf("face      : %c (%s)%s\n", app.style.id, app.style.name, app.pattern ? "  [test pattern on]" : "");
  Serial.printf("mode      : %s\n", app.buddy.autoMode() ? "autonomous" : "manual");
  if (a)
    Serial.printf("playing   : #%d %s  %u / %u ms%s\n", a->id, a->name, app.buddy.animElapsed(now),
                  scaledDuration(*a, app.buddy.variant()), app.buddy.variant().mirror ? " (mirrored)" : "");
  else if (app.buddy.autoMode())
    Serial.printf("playing   : idle (next autonomous pick in ~%u s)\n", app.buddy.nextAutoIn(now) / 1000);
  else
    Serial.printf("playing   : idle\n");
  Serial.printf("display   : %s @ 0x%02X, SDA %u, SCL %u, %lu Hz\n", app.display.name(), cfg::kI2cAddr, cfg::kSda,
                cfg::kScl, (unsigned long)app.display.busHz());
  Serial.printf("uptime    : %luh %02lum %02lus, %lu frames\n", (unsigned long)(up / 3600), (unsigned long)(up / 60 % 60),
                (unsigned long)(up % 60), (unsigned long)app.stats.total);
}

void fps() {
  const FrameStats& s = app.stats;
  Serial.printf("cadence   : %.1f fps (target %.1f)\n", s.fps, 1000.0 / cfg::kFrameMs);
  Serial.printf("render    : avg %lu us, max %lu us  (animation + face rasterisation)\n", (unsigned long)s.renderAvgUs,
                (unsigned long)s.renderMaxUs);
  Serial.printf("transfer  : avg %lu us, max %lu us  (1024-byte frame over I2C)\n", (unsigned long)s.presentAvgUs,
                (unsigned long)s.presentMaxUs);
}

void memory() {
  Serial.printf("heap free : %lu bytes (min since boot %lu, min seen in frame loop %lu)\n",
                (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMinFreeHeap(), (unsigned long)app.stats.minFreeHeap);
  Serial.printf("heap block: %lu bytes largest allocatable\n", (unsigned long)ESP.getMaxAllocHeap());
  Serial.printf("heap total: %lu bytes\n", (unsigned long)ESP.getHeapSize());
  Serial.printf("sketch    : %lu bytes used, %lu bytes free in app partition\n", (unsigned long)ESP.getSketchSize(),
                (unsigned long)ESP.getFreeSketchSpace());
  Serial.printf("flash chip: %lu bytes, %lu Hz\n", (unsigned long)ESP.getFlashChipSize(),
                (unsigned long)ESP.getFlashChipSpeed());
  Serial.printf("framebuf  : 1024 bytes (single buffer, owned by U8g2, rendered in place)\n");
}

void probe() {
  Serial.println("I2C scan:");
  int found = 0;
  for (uint8_t a = 1; a < 127; ++a)
    if (app.display.ack(a)) { Serial.printf("  device at 0x%02X\n", a); ++found; }
  if (!found) Serial.println("  nothing answered - check wiring, 3V3/GND and the SDA/SCL pins in Config.h");
  int st = app.display.readStatus();
  if (st < 0) {
    Serial.println("status byte: no answer");
    return;
  }
  Serial.printf("status byte: 0x%02X -> heuristic guess: %s\n", st, (st & 0x0F) == 0x08 || st == 0x00 ? "SH1106" : "SSD1306");
  Serial.println("  (heuristic only - confirm with `pattern`: a correct controller shows a clean 1px border");
  Serial.println("   touching all four edges; the wrong one shows a shifted frame or a noisy right edge)");
}

}  // namespace

void SerialCommands::printHelp() {
  Serial.println(F(
      "TinyBuddy commands (case-insensitive):\n"
      "  help                    this list\n"
      "  list                    implemented animations\n"
      "  play <id|name> [m]      play one animation (m = mirrored), e.g. `play 80`, `play finale`\n"
      "  random                  play a random animation\n"
      "  auto on|off             autonomous behaviour (off = manual mode)\n"
      "  idle | stop             return to idle (stop = safe interruption)\n"
      "  status | fps | memory   diagnostics\n"
      "  faces                   list the six design candidates\n"
      "  face <A-F>              switch candidate live (saved)\n"
      "  set <param> <value>     live style tweak, e.g. `set pupilRx 14` (not saved)\n"
      "  get <param>             read a style parameter\n"
      "  display ssd1306|sh1106  switch OLED controller (saved, applied immediately)\n"
      "  probe                   I2C scan + controller guess\n"
      "  pattern                 toggle controller test pattern\n"
      "  contrast <0-255>        panel contrast\n"
      "  i2c <hz>                I2C clock for this session (100000..1000000)\n"
      "  seed <n>                reseed the behaviour RNG (reproducible runs)\n"
      "  test all                play every animation (both variants) and report\n"
      "  reboot"));
}

void SerialCommands::poll(uint32_t now) {
  while (Serial.available() > 0) {
    int c = Serial.read();
    if (c < 0) break;
    if (c == '\r' || c == '\n') {
      if (overflow_) {
        Serial.println("error: line too long (max 95 characters)");
      } else if (len_ > 0) {
        line_[len_] = '\0';
        exec(line_, now);
      }
      len_ = 0;
      overflow_ = false;
    } else if (len_ < sizeof(line_) - 1) {
      line_[len_++] = (char)c;
    } else {
      overflow_ = true;  // keep discarding until end of line
    }
  }
}

void SerialCommands::exec(char* line, uint32_t now) {
  char* argv[6] = {};
  int argc = 0;
  for (char* tok = strtok(line, " \t"); tok && argc < 6; tok = strtok(nullptr, " \t")) argv[argc++] = tok;
  if (!argc) return;
  const char* cmd = argv[0];
  long v;

  if (is(cmd, "help") || is(cmd, "?")) return printHelp();
  if (is(cmd, "list")) return listAnimations();
  if (is(cmd, "faces")) return listFaces();
  if (is(cmd, "status")) return status(now);
  if (is(cmd, "fps")) return fps();
  if (is(cmd, "memory") || is(cmd, "mem")) return memory();
  if (is(cmd, "probe")) return probe();

  if (is(cmd, "play")) {
    if (argc < 2) return (void)Serial.println("usage: play <id|name> [m]");
    const AnimDef* a = parseUInt(argv[1], v, 0, 255) ? animById((int)v) : nullptr;
    if (!a) {
      // allow multi-word names: join the remaining tokens
      static char name[64];
      name[0] = '\0';
      int last = argc;
      bool mirrored = argc > 2 && is(argv[argc - 1], "m");
      if (mirrored) --last;
      for (int i = 1; i < last; ++i) {
        if (i > 1) strlcat(name, " ", sizeof name);
        strlcat(name, argv[i], sizeof name);
      }
      a = animByName(name);
    }
    if (!a) return (void)Serial.printf("unknown animation '%s' - try `list`\n", argv[1]);
    bool mirrored = argc > 2 && is(argv[argc - 1], "m");
    app.pattern = false;
    if (app.buddy.play(*a, now, mirrored ? 1 : 0)) Serial.printf("playing #%d %s%s\n", a->id, a->name, mirrored ? " (mirrored)" : "");
    else Serial.println("busy: current animation is not interruptible");
    return;
  }
  if (is(cmd, "random")) {
    const AnimDef& a = animAt((int)(esp_random() % (uint32_t)animCount()));
    app.buddy.play(a, now);
    Serial.printf("playing #%d %s\n", a.id, a.name);
    return;
  }
  if (is(cmd, "auto")) {
    if (argc < 2 || !(is(argv[1], "on") || is(argv[1], "off"))) return (void)Serial.println("usage: auto on|off");
    app.buddy.setAuto(is(argv[1], "on"), now);
    app.savePrefs();
    Serial.printf("autonomous mode %s\n", app.buddy.autoMode() ? "ON" : "OFF (manual)");
    return;
  }
  if (is(cmd, "idle") || is(cmd, "stop")) {
    app.buddy.stop(now);
    app.pattern = false;
    Serial.println("idle");
    return;
  }
  if (is(cmd, "face")) {
    if (argc < 2 || !styleById(argv[1][0]) || argv[1][1]) return (void)Serial.println("usage: face <A-F>  (see `faces`)");
    app.selectStyle(argv[1][0]);
    app.savePrefs();
    Serial.printf("face %c (%s)\n", app.style.id, app.style.name);
    return;
  }
  if (is(cmd, "set")) {
    if (argc < 3 || !setStyleParam(app.style, argv[1], argv[2]))
      return (void)Serial.println("usage: set <param> <value>   e.g. set eyeW 32 | set highlight dots | set rest.open 0.8");
    char out[24];
    getStyleParam(app.style, argv[1], out, sizeof out);
    Serial.printf("%s = %s (live, not saved; `face %c` resets)\n", argv[1], out, app.style.id);
    return;
  }
  if (is(cmd, "get")) {
    char out[24];
    if (argc < 2 || !getStyleParam(app.style, argv[1], out, sizeof out)) return (void)Serial.println("usage: get <param>");
    Serial.printf("%s = %s\n", argv[1], out);
    return;
  }
  if (is(cmd, "display")) {
    Controller c;
    if (argc >= 2 && is(argv[1], "ssd1306")) c = Controller::SSD1306;
    else if (argc >= 2 && is(argv[1], "sh1106")) c = Controller::SH1106;
    else return (void)Serial.printf("usage: display ssd1306|sh1106   (now: %s)\n", app.display.name());
    bool ok = app.display.begin(c);
    app.savePrefs();
    Serial.printf("display -> %s %s\n", app.display.name(), ok ? "ok" : "(no ACK from the panel!)");
    return;
  }
  if (is(cmd, "pattern")) {
    app.pattern = !app.pattern;
    Serial.printf("test pattern %s\n", app.pattern ? "ON (1px border, corner ticks, centre cross)" : "off");
    return;
  }
  if (is(cmd, "contrast")) {
    if (argc < 2 || !parseUInt(argv[1], v, 0, 255)) return (void)Serial.println("usage: contrast <0-255>");
    app.display.setContrast((uint8_t)v);
    Serial.printf("contrast %ld\n", v);
    return;
  }
  if (is(cmd, "i2c")) {
    if (argc < 2 || !parseUInt(argv[1], v, 100000, 1000000)) return (void)Serial.println("usage: i2c <100000..1000000>");
    app.display.setBusHz((uint32_t)v);
    Serial.printf("I2C clock %ld Hz (session only) - check `fps`\n", v);
    return;
  }
  if (is(cmd, "seed")) {
    if (argc < 2 || !parseUInt(argv[1], v, 0, 0x7FFFFFFF)) return (void)Serial.println("usage: seed <n>");
    app.buddy.begin(app.style, (uint32_t)v, now);
    Serial.printf("behaviour reseeded with %ld\n", v);
    return;
  }
  if (is(cmd, "test")) {
    if (argc < 2 || !is(argv[1], "all")) return (void)Serial.println("usage: test all");
    app.test.start(now);
    return;
  }
  if (is(cmd, "reboot")) {
    Serial.println("rebooting...");
    Serial.flush();
    delay(50);
    ESP.restart();
  }
  Serial.printf("unknown command '%s' - type `help`\n", cmd);
}
