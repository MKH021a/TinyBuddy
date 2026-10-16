// TinyBuddy firmware — milestone 1.
// Runs the shared core (tb_*): idle behaviour, animation #80 and all six face candidates,
// rendered into the OLED driver's framebuffer at ~30 fps without blocking the serial console.
#include <Arduino.h>
#include <Preferences.h>
#include <esp_random.h>

#include "App.h"
#include "SerialCommands.h"
#include "tb_canvas.h"
#include "tb_catalog.h"

using namespace tb;

App app;
static SerialCommands console;
static Preferences prefs;

// ------------------------------------------------------------------- App
void App::selectStyle(char id) {
  const FaceStyle* s = styleById(id);
  if (!s) return;
  styleId = s->id;
  style = *s;  // fresh copy: drops any live `set` tweaks
  buddy.setStyle(style);
}

void App::savePrefs() {
  prefs.begin("tinybuddy", false);
  prefs.putChar("face", styleId);
  prefs.putUChar("ctrl", (uint8_t)display.controller());
  prefs.putBool("auto", buddy.autoMode());
  prefs.end();
}

void FrameStats::add(uint32_t nowMs, uint32_t renderUs, uint32_t presentUs) {
  ++total;
  ++winFrames_;
  rSum_ += renderUs;
  pSum_ += presentUs;
  if (renderUs > rMax_) rMax_ = renderUs;
  if (presentUs > pMax_) pMax_ = presentUs;
  uint32_t heap = ESP.getFreeHeap();
  if (heap < minFreeHeap) minFreeHeap = heap;
  uint32_t span = nowMs - winStart_;
  if (span >= 1000) {
    fps = winFrames_ * 1000.f / span;
    renderAvgUs = (uint32_t)(rSum_ / winFrames_);
    presentAvgUs = (uint32_t)(pSum_ / winFrames_);
    renderMaxUs = rMax_;
    presentMaxUs = pMax_;
    winStart_ = nowMs;
    winFrames_ = 0;
    rSum_ = pSum_ = 0;
    rMax_ = pMax_ = 0;
  }
}

// ------------------------------------------------------------------- test all
void TestRunner::start(uint32_t now) {
  active = true;
  index_ = mirror_ = failures_ = runs_ = 0;
  wasAuto_ = app.buddy.autoMode();
  app.pattern = false;
  app.buddy.begin(app.style, 12345, now);  // deterministic seed
  app.buddy.setAuto(false, now);
  Serial.printf("test all: %d animation(s) x 2 variants, seed 12345\n", animCount());
  next(now);
}

void TestRunner::next(uint32_t now) {
  if (index_ >= animCount()) {
    Serial.printf("test all: %d runs, %d failed, min free heap %lu, worst render %lu us\n", runs_, failures_,
                  (unsigned long)app.stats.minFreeHeap, (unsigned long)worstRenderUs_);
    app.buddy.setAuto(wasAuto_, now);
    active = false;
    return;
  }
  const AnimDef& a = animAt(index_);
  app.buddy.play(a, now, mirror_);
  started_ = now;
  expected_ = scaledDuration(a, app.buddy.variant());
  worstRenderUs_ = 0;
}

void TestRunner::update(uint32_t now) {
  if (!active) return;
  if (app.lastFrameRenderUs > worstRenderUs_) worstRenderUs_ = app.lastFrameRenderUs;
  if (app.buddy.current()) return;  // still playing
  const AnimDef& a = animAt(index_);
  uint32_t took = now - started_;
  bool timingOk = took + 70 >= expected_ && took <= expected_ + 70;  // within ~2 frames
  bool clean = app.buddy.fx().n == 0;
  ++runs_;
  if (!timingOk || !clean) ++failures_;
  Serial.printf("  %s #%d %s%s: %lu ms (expected %lu), effects cleared: %s, worst render %lu us\n",
                timingOk && clean ? "PASS" : "FAIL", a.id, a.name, mirror_ ? " [mirrored]" : "", (unsigned long)took,
                (unsigned long)expected_, clean ? "yes" : "NO", (unsigned long)worstRenderUs_);
  if (++mirror_ > 1) { mirror_ = 0; ++index_; }
  next(now);
}

// ------------------------------------------------------------------- display test pattern
static void drawPattern(Canvas& c) {
  c.clear();
  c.frame(0, 0, kW, kH, Ink::On);                  // must touch all four edges exactly
  for (int i = 0; i < 6; ++i) {                    // corner ticks
    c.pixel(2 + i, 2, Ink::On);   c.pixel(2, 2 + i, Ink::On);
    c.pixel(kW - 3 - i, 2, Ink::On); c.pixel(kW - 3, 2 + i, Ink::On);
    c.pixel(2 + i, kH - 3, Ink::On); c.pixel(2, kH - 3 - i, Ink::On);
    c.pixel(kW - 3 - i, kH - 3, Ink::On); c.pixel(kW - 3, kH - 3 - i, Ink::On);
  }
  c.hspan(kW / 2 - 8, kW / 2 + 7, kH / 2, Ink::On);  // centre cross
  c.vspan(kW / 2, kH / 2 - 8, kH / 2 + 7, Ink::On);
  for (int x = 8; x < kW - 8; x += 8) c.pixel(x, kH - 5, Ink::On);  // 8px ruler
}

// ------------------------------------------------------------------- Arduino entry points
void setup() {
  Serial.begin(cfg::kSerialBaud);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 1500) delay(10);  // native USB: give the host a moment (bounded)

  prefs.begin("tinybuddy", true);
  char face = (char)prefs.getChar("face", cfg::kDefaultFace);
  uint8_t ctrl = prefs.getUChar("ctrl", TB_DEFAULT_CONTROLLER);
  bool autoOn = prefs.getBool("auto", true);
  prefs.end();

  Serial.println();
  Serial.println("TinyBuddy  -  milestone 1 (six face candidates + animation #80)");
  bool ok = app.display.begin(ctrl ? Controller::SH1106 : Controller::SSD1306);
  Serial.printf("display: %s at 0x%02X on SDA %u / SCL %u, %lu Hz: %s\n", app.display.name(), cfg::kI2cAddr, cfg::kSda,
                cfg::kScl, (unsigned long)app.display.busHz(),
                ok ? "OK" : "NO ACK - check wiring/address (rendering continues; try `probe`)");
  if (!styleById(face)) face = cfg::kDefaultFace;
  app.selectStyle(face);
  app.buddy.begin(app.style, esp_random(), millis());  // hardware RNG seed in normal mode
  app.buddy.setAuto(autoOn, millis());
  Serial.printf("face %c (%s), %s mode. Free heap %lu bytes. Type `help`.\n", app.style.id, app.style.name,
                autoOn ? "autonomous" : "manual", (unsigned long)ESP.getFreeHeap());
}

void loop() {
  static uint32_t nextFrame = 0;
  const uint32_t now = millis();
  console.poll(now);
  app.test.update(now);
  if ((int32_t)(now - nextFrame) < 0) {
    delay(1);  // yield to the RTOS idle task between frames; console is polled every ms
    return;
  }
  nextFrame += cfg::kFrameMs;
  if ((int32_t)(now - nextFrame) > (int32_t)cfg::kFrameMs) nextFrame = now + cfg::kFrameMs;  // resync after a stall

  const uint32_t a = micros();
  Canvas canvas(app.display.buffer());
  if (app.pattern) {
    drawPattern(canvas);
  } else {
    app.buddy.update(now);
    app.buddy.render(canvas);
  }
  const uint32_t b = micros();
  app.display.present();
  const uint32_t c = micros();
  app.lastFrameRenderUs = b - a;
  app.stats.add(now, b - a, c - b);
}
