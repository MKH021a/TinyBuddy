// Automated QA for the TinyBuddy core. Runs natively; exercises the exact firmware code paths.
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <string>
#include <vector>

#include "sim.h"
#include "tb_buddy.h"
#include "tb_catalog.h"
#include "tb_params.h"

using namespace tb;

namespace sim {

namespace {

struct Result {
  std::string name;
  bool pass;
  std::string detail;
};
std::vector<Result> gResults;

void report(const std::string& name, bool pass, const std::string& detail) {
  gResults.push_back({name, pass, detail});
}

std::string fmt(const char* f, ...) __attribute__((format(printf, 1, 2)));
std::string fmt(const char* f, ...) {
  char b[512];
  va_list ap;
  va_start(ap, f);
  vsnprintf(b, sizeof b, f, ap);
  va_end(ap);
  return b;
}

int isolatedLit(const Canvas& c) {
  int n = 0;
  for (int y = 0; y < kH; ++y)
    for (int x = 0; x < kW; ++x) {
      if (!c.get(x, y)) continue;
      bool nb = false;
      for (int j = -1; j <= 1 && !nb; ++j)
        for (int i = -1; i <= 1 && !nb; ++i)
          if ((i || j) && c.get(x + i, y + j)) nb = true;
      if (!nb) ++n;
    }
  return n;
}

int edgeLit(const Canvas& c) {
  int n = 0;
  for (int x = 0; x < kW; ++x) n += c.get(x, 0) + c.get(x, kH - 1);
  for (int y = 1; y < kH - 1; ++y) n += c.get(0, y) + c.get(kW - 1, y);
  return n;
}

int diffPixels(const uint8_t* a, const uint8_t* b) {
  int n = 0;
  for (int i = 0; i < kBufBytes; ++i) {
    uint8_t d = a[i] ^ b[i];
    while (d) { d &= (uint8_t)(d - 1); ++n; }
  }
  return n;
}

void testCatalog() {
  bool ok = true;
  std::string why;
  for (int i = 0; i < animCount(); ++i) {
    const AnimDef& a = animAt(i);
    for (int j = i + 1; j < animCount(); ++j) {
      if (animAt(j).id == a.id) { ok = false; why += fmt("dup id %d; ", a.id); }
      if (!strcmp(animAt(j).name, a.name)) { ok = false; why += fmt("dup name %s; ", a.name); }
    }
    for (int k = 0; k < a.nKeys; ++k) {
      const Key& key = a.keys[k];
      if (k && key.t < a.keys[k - 1].t) { ok = false; why += fmt("#%d key %d out of order; ", a.id, k); }
      if (key.t + key.d > a.duration) { ok = false; why += fmt("#%d key %d ends after duration; ", a.id, k); }
      if ((int)key.ch >= kChannelCount && (uint8_t)key.ch < 64) { ok = false; why += "bad channel; "; }
    }
    for (int k = 0; k < a.nFx; ++k)
      if (a.fx[k].t + a.fx[k].d > a.duration) { ok = false; why += fmt("#%d fx %d outlives animation; ", a.id, k); }
    for (int k = 1; k < a.nMarkers; ++k)
      if (a.markers[k].t < a.markers[k - 1].t) { ok = false; why += "markers unsorted; "; }
    if (animById(a.id) != &a) { ok = false; why += fmt("lookup by id %d; ", a.id); }
    if (animByName(a.name) != &a) { ok = false; why += fmt("lookup by name %s; ", a.name); }
  }
  report("catalog integrity", ok, ok ? fmt("%d animation(s) registered, all keys ordered and inside duration", animCount()) : why);
  report("catalog size (milestone 1 = 1 of 80 planned)", animCount() >= 1, fmt("%d/80 implemented", animCount()));
}

void testAnimations() {
  uint8_t buf[kBufBytes], ref[kBufBytes], prev[kBufBytes];
  Canvas c(buf), r(ref);
  for (int ai = 0; ai < animCount(); ++ai) {
    const AnimDef& a = animAt(ai);
    for (int si = 0; si < styleCount(); ++si) {
      const FaceStyle& st = styleAt(si);
      for (int m = 0; m < 2; ++m) {
        Variant v;
        v.mirror = m == 1;
        v.seed = 1234u + (uint32_t)si;
        FaceState f;
        FxList fx;
        int maxIso = 0, maxEdge = 0, maxJump = 0, frames = 0;
        float overflow = 0;
        uint32_t dur = scaledDuration(a, v);
        bool havePrev = false;
        for (uint32_t t = 0; t <= dur; t += 1000 / 60) {
          evaluate(a, t, v, st, st.rest, f, fx);
          renderFace(c, st, f, nullptr);  // without effects: face geometry only
          maxIso = imax(maxIso, isolatedLit(c));
          maxEdge = imax(maxEdge, edgeLit(c));
          for (int e = 0; e < 2; ++e) {  // horizontal: the safe area only corrects vertically
            EyeBox b = eyeBox(st, f, e);
            overflow = fmaxf2(overflow, fmaxf2(-(b.cx - b.a), b.cx + b.a - kW));
          }
          renderFace(c, st, f, &fx);
          if (havePrev) maxJump = imax(maxJump, diffPixels(prev, buf));
          memcpy(prev, buf, kBufBytes);
          havePrev = true;
          ++frames;
        }
        // ending: the final frame must be pixel-identical to the resting face, with no effects
        evaluate(a, dur, v, st, st.rest, f, fx);
        renderFace(c, st, f, &fx);
        renderFace(r, st, st.rest, nullptr);
        int endDiff = diffPixels(buf, ref);
        std::string tag = fmt("#%d %s face %c%s", a.id, a.name, st.id, m ? " (mirrored)" : "");
        report(tag + ": ends on resting face, no leftover effects", endDiff == 0 && fx.n == 0,
               fmt("%d px differ, %d effects at end", endDiff, fx.n));
        report(tag + ": eyes stay on screen horizontally", overflow <= 1.0f, fmt("max eye-box overflow %.2f px", (double)overflow));
        report(tag + ": no stray pixels / edge clipping", maxIso <= 2 && maxEdge == 0,
               fmt("max isolated lit px %d, max lit px on screen border %d, %d frames, max frame-to-frame change %d px",
                   maxIso, maxEdge, frames, maxJump));
      }
    }
  }
}

uint32_t reelHash(const FaceStyle& st, uint32_t seed, uint32_t t0, uint32_t ms, int playAt, int* plays = nullptr) {
  Buddy b;
  b.begin(st, seed, t0);
  uint8_t buf[kBufBytes];
  Canvas c(buf);
  uint32_t h = 2166136261u;
  bool played = playAt < 0;
  for (uint32_t t = 0; t < ms; t += 33) {
    if (!played && (int)t >= playAt) { b.play(animAt(0), t0 + t, 0); played = true; }
    b.update(t0 + t);
    b.render(c);
    h = fnv1a(buf, kBufBytes, h);
  }
  if (plays) *plays = (int)b.playsStarted();
  return h;
}

void testDeterminismAndRollover() {
  const FaceStyle& st = styleAt(5);
  uint32_t a = reelHash(st, 42, 0, 20000, 1500);
  uint32_t b = reelHash(st, 42, 0, 20000, 1500);
  report("determinism: same seed -> identical 20 s reel", a == b, fmt("hash %08x vs %08x", a, b));
  uint32_t c = reelHash(st, 43, 0, 20000, 1500);
  report("variation: different seed -> different reel", a != c, fmt("hash %08x vs %08x", a, c));
  uint32_t t0 = 0xFFFFFFFFu - 4000u;  // millis() wraps 4 s into the run
  uint32_t d = reelHash(st, 42, t0, 20000, 1500);
  report("millis() rollover: reel across the 32-bit wrap is identical", a == d, fmt("hash %08x vs %08x", a, d));
}

void testInterruption() {
  const FaceStyle& st = styleAt(5);
  bool ok = true;
  std::string why;
  for (uint32_t stopAt : {300u, 1600u, 2600u, 3100u, 4300u}) {  // blink, look, POP+fx, bounce, wink
    Buddy b;
    b.begin(st, 9, 0);
    b.setAuto(false, 0);
    b.play(animAt(0), 1000, 0);
    for (uint32_t t = 1000; t < 1000 + stopAt; t += 16) b.update(t);
    b.stop(1000 + stopAt);
    b.update(1000 + stopAt + 16);
    if (b.fx().n != 0) { ok = false; why += fmt("fx left after stop at %u; ", stopAt); }
    for (uint32_t t = 1000 + stopAt + 16; t < 1000 + stopAt + 400; t += 16) b.update(t);
    const FaceState& f = b.face();
    if (b.current() || fabsf(f.mouthTongue - st.rest.mouthTongue) > 1e-3f || fabsf(f.blush - st.rest.blush) > 1e-3f ||
        fabsf(f.squash) > 1e-3f || fabsf(f.eye[0].scaleW - 1.f) > 1e-3f || fabsf(f.faceX) > 1e-3f) {
      ok = false;
      why += fmt("not back to idle 400 ms after stop at %u; ", stopAt);
    }
  }
  report("interruption: stop mid-animation clears effects and recovers to idle", ok,
         ok ? "stopped during blink, look, POP (hearts), bounce, wink" : why);
}

void testAutonomous() {
  const FaceStyle& st = styleAt(5);
  int plays = 0;
  reelHash(st, 5, 0, 90000, -1, &plays);
  report("autonomous mode: plays on its own with idle gaps", plays >= 3 && plays <= 8,
         fmt("%d plays in 90 s (expect ~4-6 with 9-16 s idle gaps)", plays));
}

void testParams() {
  FaceStyle st = styleAt(0);
  char v[32];
  bool ok = setStyleParam(st, "eyeW", "33.5") && getStyleParam(st, "eyeW", v, sizeof v) && !strcmp(v, "33.5") &&
            setStyleParam(st, "shape", "squircle") && getStyleParam(st, "shape", v, sizeof v) && !strcmp(v, "squircle") &&
            setStyleParam(st, "rest.open", "0.5") && fabsf(st.rest.eye[1].open - 0.5f) < 1e-6f &&
            !setStyleParam(st, "nonsense", "1") && !setStyleParam(st, "shape", "triangle");
  report("style parameter reflection (set/get/reject)", ok, ok ? "floats, enums, groups, rejects bad input" : "mismatch");
}

void testPerformance() {
  const FaceStyle& st = styleAt(5);
  const AnimDef& a = animAt(0);
  uint8_t buf[kBufBytes];
  Canvas c(buf);
  FaceState f;
  FxList fx;
  Variant v;
  auto t0 = std::chrono::steady_clock::now();
  int n = 0;
  for (int rep = 0; rep < 20; ++rep)
    for (uint32_t t = 0; t <= a.duration; t += 33, ++n) {
      evaluate(a, t, v, st, st.rest, f, fx);
      renderFace(c, st, f, &fx);
    }
  double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count() / n;
  report("host render cost (informational; device timing measured on hardware)", true,
         fmt("%.1f us per frame (evaluate + render) over %d frames on this Mac", us, n));
}

}  // namespace

int cmdTest(const std::vector<std::string>& a) {
  bool json = opt(a, "--json", "") != "" || (a.size() > 1 && a[1] == "--json");
  testCatalog();
  testAnimations();
  testDeterminismAndRollover();
  testInterruption();
  testAutonomous();
  testParams();
  testPerformance();
  int fails = 0;
  for (const Result& r : gResults) fails += !r.pass;
  if (json) {
    printf("{\"passed\":%zu,\"failed\":%d,\"results\":[", gResults.size() - fails, fails);
    for (size_t i = 0; i < gResults.size(); ++i)
      printf("%s{\"name\":\"%s\",\"pass\":%s,\"detail\":\"%s\"}", i ? "," : "", gResults[i].name.c_str(),
             gResults[i].pass ? "true" : "false", gResults[i].detail.c_str());
    printf("]}\n");
  } else {
    for (const Result& r : gResults) printf("%s  %s\n      %s\n", r.pass ? "PASS" : "FAIL", r.name.c_str(), r.detail.c_str());
    printf("\n%zu checks, %d failed\n", gResults.size(), fails);
  }
  return fails ? 1 : 0;
}

}  // namespace sim
