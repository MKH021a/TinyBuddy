// Animation export: deterministic frame sequences for the previewer, GIFs and QA.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "sim.h"
#include "tb_buddy.h"
#include "tb_catalog.h"

using namespace tb;

namespace sim {

namespace {

struct Seq {
  int fps = 30;
  std::vector<uint8_t> frames;  // count * 1024, page layout (exactly what the OLED receives)
  std::vector<uint32_t> t;      // ms per frame
  std::vector<int> playing;     // animation id per frame (0 = idle)
};

void push(Seq& s, const Canvas& c, uint32_t t, int playing) {
  s.frames.insert(s.frames.end(), c.data(), c.data() + kBufBytes);
  s.t.push_back(t);
  s.playing.push_back(playing);
}

bool produceAnim(const std::vector<std::string>& a, size_t at, Seq& s, const AnimDef*& outA, FaceStyle& st,
                 Variant& v) {
  int id = atoi(a[at].c_str());
  const AnimDef* an = animById(id);
  if (!an) an = animByName(a[at].c_str());
  int si = styleIndex(a[at + 1][0]);
  if (!an || si < 0) { fprintf(stderr, "unknown animation or face\n"); return false; }
  st = styled(si);
  s.fps = atoi(opt(a, "--fps", "30").c_str());
  if (s.fps <= 0 || s.fps > 240) s.fps = 30;
  v.mirror = atoi(opt(a, "--mirror", "0").c_str()) != 0;
  v.timeScale = (float)atof(opt(a, "--tscale", "1").c_str());
  v.seed = (uint32_t)strtoul(opt(a, "--seed", "1").c_str(), nullptr, 10);
  uint32_t dur = scaledDuration(*an, v);
  uint8_t buf[kBufBytes];
  Canvas c(buf);
  FaceState f;
  FxList fx;
  int n = (int)ceil(dur * s.fps / 1000.0);
  for (int i = 0; i <= n; ++i) {
    uint32_t t = (uint32_t)llround(i * 1000.0 / s.fps);
    if (t > dur) t = dur;
    evaluate(*an, t, v, st, st.rest, f, fx);
    renderFace(c, st, f, &fx);
    push(s, c, t, an->id);
  }
  outA = an;
  return true;
}

bool produceReel(const std::vector<std::string>& a, size_t at, Seq& s, FaceStyle& st) {
  int si = styleIndex(a[at][0]);
  if (si < 0) { fprintf(stderr, "unknown face\n"); return false; }
  st = styled(si);
  double seconds = atof(a[at + 1].c_str());
  s.fps = atoi(opt(a, "--fps", "30").c_str());
  if (s.fps <= 0 || s.fps > 240) s.fps = 30;
  uint32_t seed = (uint32_t)strtoul(opt(a, "--seed", "7").c_str(), nullptr, 10);
  int first = atoi(opt(a, "--first", "2500").c_str());  // ms; -1 = wait for autonomous pick
  uint32_t t0 = (uint32_t)strtoul(opt(a, "--t0", "0").c_str(), nullptr, 10);  // start clock (rollover tests)
  Buddy b;
  b.begin(st, seed, t0);
  uint8_t buf[kBufBytes];
  Canvas c(buf);
  int n = (int)(seconds * s.fps);
  bool firstDone = first < 0;
  for (int i = 0; i < n; ++i) {
    uint32_t t = (uint32_t)llround(i * 1000.0 / s.fps);
    uint32_t now = t0 + t;
    if (!firstDone && (int)t >= first) { b.play(animAt(0), now); firstDone = true; }
    b.update(now);
    b.render(c);
    push(s, c, t, b.current() ? b.current()->id : 0);
  }
  return true;
}

bool writeSeq(const std::string& dir, const Seq& s, const std::string& metaExtra) {
  std::string bin = dir + "/frames.bin";
  FILE* f = fopen(bin.c_str(), "wb");
  if (!f) { perror(bin.c_str()); return false; }
  fwrite(s.frames.data(), 1, s.frames.size(), f);
  fclose(f);
  std::string js = dir + "/frames.json";
  f = fopen(js.c_str(), "w");
  if (!f) { perror(js.c_str()); return false; }
  fprintf(f, "{\"fps\":%d,\"count\":%zu,%s\"t\":[", s.fps, s.t.size(), metaExtra.c_str());
  for (size_t i = 0; i < s.t.size(); ++i) fprintf(f, "%s%u", i ? "," : "", s.t[i]);
  fprintf(f, "],\"playing\":[");
  for (size_t i = 0; i < s.playing.size(); ++i) fprintf(f, "%s%d", i ? "," : "", s.playing[i]);
  fprintf(f, "]}\n");
  fclose(f);
  return true;
}

std::string animMeta(const AnimDef& an, const FaceStyle& st, const Variant& v) {
  std::string m = "\"id\":" + std::to_string(an.id) + ",\"name\":\"" + an.name + "\",\"face\":\"" + st.id +
                  "\",\"mirror\":" + (v.mirror ? "1" : "0") + ",\"duration\":" +
                  std::to_string(scaledDuration(an, v)) + ",\"markers\":[";
  for (int i = 0; i < an.nMarkers; ++i)
    m += std::string(i ? "," : "") + "{\"t\":" + std::to_string((uint32_t)(an.markers[i].t * v.timeScale)) +
         ",\"label\":\"" + an.markers[i].label + "\"}";
  return m + "],";
}

void printJson(const Seq& s, const std::string& meta) {
  printf("{\"fps\":%d,\"count\":%zu,%s\"t\":[", s.fps, s.t.size(), meta.c_str());
  for (size_t i = 0; i < s.t.size(); ++i) printf("%s%u", i ? "," : "", s.t[i]);
  printf("],\"playing\":[");
  for (size_t i = 0; i < s.playing.size(); ++i) printf("%s%d", i ? "," : "", s.playing[i]);
  printf("],\"frames\":[");
  for (size_t i = 0; i < s.t.size(); ++i)
    printf("%s\"%s\"", i ? "," : "", base64(&s.frames[i * kBufBytes], kBufBytes).c_str());
  printf("]}\n");
}

}  // namespace

int cmdAnim(const std::vector<std::string>& a) {
  Seq s;
  const AnimDef* an = nullptr;
  FaceStyle st;
  Variant v;
  if (!produceAnim(a, 1, s, an, st, v)) return 1;
  return writeSeq(a[3], s, animMeta(*an, st, v)) ? 0 : 1;
}

int cmdReel(const std::vector<std::string>& a) {
  Seq s;
  FaceStyle st;
  if (!produceReel(a, 1, s, st)) return 1;
  return writeSeq(a[3], s, std::string("\"face\":\"") + st.id + "\",") ? 0 : 1;
}

int cmdJson(const std::vector<std::string>& a) {
  Seq s;
  FaceStyle st;
  if (a[1] == "anim" && a.size() >= 4) {
    const AnimDef* an = nullptr;
    Variant v;
    if (!produceAnim(a, 2, s, an, st, v)) return 1;
    printJson(s, animMeta(*an, st, v));
    return 0;
  }
  if (a[1] == "reel" && a.size() >= 4) {
    if (!produceReel(a, 2, s, st)) return 1;
    printJson(s, std::string("\"face\":\"") + st.id + "\",");
    return 0;
  }
  if (a[1] == "pose" && a.size() >= 4) {  // json pose <face> <expr|rest>
    int si = styleIndex(a[2][0]);
    if (si < 0) return 1;
    st = styled(si);
    FaceState f = st.rest;
    if (a[3] != "rest") {
      const Expression* ex = expressionByName(a[3].c_str());
      if (!ex) { fprintf(stderr, "unknown expression\n"); return 1; }
      applyExpression(*ex, f);
    }
    uint8_t buf[kBufBytes];
    Canvas c(buf);
    renderFace(c, st, f, nullptr);
    printf("{\"face\":\"%c\",\"expr\":\"%s\",\"frame\":\"%s\"}\n", st.id, a[3].c_str(), base64(buf, kBufBytes).c_str());
    return 0;
  }
  fprintf(stderr, "usage: tbsim json anim <id> <face> [...] | json reel <face> <seconds> [...] | json pose <face> <expr>\n");
  return 2;
}

}  // namespace sim
