// tbsim — native (desktop) runner for the TinyBuddy core.
// Renders with exactly the same code the ESP32 runs, and writes raw 128x64 frames.
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "sim.h"
#include "tb_anim.h"
#include "tb_catalog.h"
#include "tb_params.h"
#include "tb_render.h"

using namespace tb;

namespace sim {

std::vector<std::string> gSets;

FaceStyle styled(int i) {
  FaceStyle st = styleAt(i);
  for (const std::string& kv : gSets) {
    std::string s = kv;
    if (s.size() > 2 && s[1] == ':') {  // "A:key=val" targets one face
      if (toupper((unsigned char)s[0]) != st.id) continue;
      s = s.substr(2);
    }
    size_t eq = s.find('=');
    if (eq == std::string::npos || !setStyleParam(st, s.substr(0, eq).c_str(), s.substr(eq + 1).c_str()))
      fprintf(stderr, "warning: bad --set '%s'\n", kv.c_str());
  }
  return st;
}

int styleIndex(char id) {
  for (int i = 0; i < styleCount(); ++i)
    if (styleAt(i).id == toupper((unsigned char)id)) return i;
  return -1;
}

bool writePGM(const std::string& path, const Canvas& c) {
  FILE* f = fopen(path.c_str(), "wb");
  if (!f) { perror(path.c_str()); return false; }
  fprintf(f, "P5\n%d %d\n255\n", kW, kH);
  for (int y = 0; y < kH; ++y)
    for (int x = 0; x < kW; ++x) fputc(c.get(x, y) ? 255 : 0, f);
  fclose(f);
  return true;
}

bool writeBin(const std::string& path, const Canvas& c) {
  FILE* f = fopen(path.c_str(), "wb");
  if (!f) { perror(path.c_str()); return false; }
  fwrite(c.data(), 1, kBufBytes, f);
  fclose(f);
  return true;
}

bool writeAscii(const std::string& path, const Canvas& c) {
  FILE* f = fopen(path.c_str(), "w");
  if (!f) { perror(path.c_str()); return false; }
  for (int y = 0; y < kH; ++y) {
    for (int x = 0; x < kW; ++x) fputc(c.get(x, y) ? '#' : '.', f);
    fputc('\n', f);
  }
  fclose(f);
  return true;
}

std::string base64(const uint8_t* d, size_t n) {
  static const char* T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string o;
  o.reserve((n + 2) / 3 * 4);
  for (size_t i = 0; i < n; i += 3) {
    uint32_t v = (uint32_t)d[i] << 16 | (i + 1 < n ? (uint32_t)d[i + 1] << 8 : 0u) | (i + 2 < n ? d[i + 2] : 0u);
    o += T[(v >> 18) & 63];
    o += T[(v >> 12) & 63];
    o += i + 1 < n ? T[(v >> 6) & 63] : '=';
    o += i + 2 < n ? T[v & 63] : '=';
  }
  return o;
}

uint32_t fnv1a(const uint8_t* d, size_t n, uint32_t h) {
  for (size_t i = 0; i < n; ++i) { h ^= d[i]; h *= 16777619u; }
  return h;
}

std::string opt(const std::vector<std::string>& a, const char* key, const char* def) {
  for (size_t i = 0; i + 1 < a.size(); ++i)
    if (a[i] == key) return a[i + 1];
  return def;
}

}  // namespace sim

using namespace sim;

namespace {

void staticHearts(const FaceStyle& st, const FaceState& f, FxList& fx) {
  EyeBox l = eyeBox(st, f, 0), r = eyeBox(st, f, 1);
  fx.add(FxSprite{FxKind::Heart, (int16_t)(l.cx - l.a - 2), (int16_t)(l.cy - l.b + 2), 4, true});
  fx.add(FxSprite{FxKind::Heart, (int16_t)(r.cx + r.a + 3), (int16_t)(r.cy - r.b - 1), 3, true});
  fx.add(FxSprite{FxKind::Sparkle, (int16_t)(r.cx + r.a + 5), (int16_t)(r.cy + 7), 3, true});
}

int cmdFaces(const std::string& dir) {
  uint8_t buf[kBufBytes];
  Canvas c(buf);
  for (int i = 0; i < styleCount(); ++i) {
    const FaceStyle st = styled(i);
    renderFace(c, st, st.rest, nullptr);
    std::string base = dir + "/face_" + st.id;
    if (!writePGM(base + ".pgm", c) || !writeBin(base + ".bin", c) || !writeAscii(base + ".txt", c)) return 1;
  }
  return 0;
}

int cmdExpr(const std::string& dir) {
  uint8_t buf[kBufBytes];
  Canvas c(buf);
  for (int i = 0; i < styleCount(); ++i) {
    const FaceStyle st = styled(i);
    for (int e = 0; e < expressionCount(); ++e) {
      const Expression& ex = expressionAt(e);
      FaceState f = st.rest;
      applyExpression(ex, f);
      FxList fx;
      if (ex.fxHearts) staticHearts(st, f, fx);
      renderFace(c, st, f, &fx);
      if (!writePGM(dir + "/expr_" + st.id + "_" + ex.name + ".pgm", c)) return 1;
    }
  }
  return 0;
}

int cmdLab(const std::string& out, const std::string& face, const std::string& expr) {
  int idx = styleIndex(face.empty() ? 'F' : face[0]);
  if (idx < 0) { fprintf(stderr, "unknown face %s\n", face.c_str()); return 1; }
  FaceStyle st = styled(idx);
  FaceState f = st.rest;
  FxList fx;
  if (expr != "-" && expr != "rest") {
    const Expression* ex = expressionByName(expr.c_str());
    if (!ex) { fprintf(stderr, "unknown expression %s\n", expr.c_str()); return 1; }
    applyExpression(*ex, f);
    if (ex->fxHearts) staticHearts(st, f, fx);
  }
  uint8_t buf[kBufBytes];
  Canvas c(buf);
  renderFace(c, st, f, &fx);
  return writePGM(out, c) ? 0 : 1;
}

int cmdStyles() {
  printf("{");
  for (int i = 0; i < styleCount(); ++i) {
    FaceStyle st = styled(i);
    printf("%s\"%c\":{\"name\":\"%s\"", i ? "," : "", st.id, st.name);
    char v[32];
    for (int p = 0; p < styleParamCount(); ++p) {
      getStyleParam(st, styleParam(p).name, v, sizeof v);
      printf(",\"%s\":\"%s\"", styleParam(p).name, v);
    }
    for (int ch = 0; ch < kChannelCount; ++ch) {
      std::string key = std::string("rest.") + channelName((Ch)ch);
      getStyleParam(st, key.c_str(), v, sizeof v);
      printf(",\"%s\":%s", key.c_str(), v);
    }
    printf("}");
  }
  printf("}\n");
  return 0;
}

int cmdParams() {
  printf("[");
  for (int p = 0; p < styleParamCount(); ++p) {
    const ParamDef& d = styleParam(p);
    printf("%s{\"name\":\"%s\",\"type\":%d,\"lo\":%g,\"hi\":%g", p ? "," : "", d.name, (int)d.type, (double)d.lo,
           (double)d.hi);
    int n = enumCount(d.type);
    if (n) {
      printf(",\"options\":[");
      for (int i = 0; i < n; ++i) printf("%s\"%s\"", i ? "," : "", enumName(d.type, i));
      printf("]");
    }
    printf("}");
  }
  printf("]\n");
  return 0;
}

int cmdInfo() {
  printf("{\"faces\":[");
  for (int i = 0; i < styleCount(); ++i) {
    const FaceStyle& st = styleAt(i);
    printf("%s{\"id\":\"%c\",\"name\":\"%s\",\"motto\":\"%s\"}", i ? "," : "", st.id, st.name, st.motto);
  }
  printf("],\"expressions\":[");
  for (int e = 0; e < expressionCount(); ++e) printf("%s\"%s\"", e ? "," : "", expressionAt(e).name);
  printf("],\"animations\":[");
  for (int i = 0; i < animCount(); ++i) {
    const AnimDef& a = animAt(i);
    printf("%s{\"id\":%d,\"name\":\"%s\",\"category\":\"%s\",\"duration\":%u,\"summary\":\"%s\",\"markers\":[",
           i ? "," : "", a.id, a.name, a.category, a.duration, a.summary);
    for (int m = 0; m < a.nMarkers; ++m)
      printf("%s{\"t\":%u,\"label\":\"%s\"}", m ? "," : "", a.markers[m].t, a.markers[m].label);
    printf("]}");
  }
  printf("]}\n");
  return 0;
}

void usage() {
  fprintf(stderr,
          "tbsim — TinyBuddy native renderer (same core as the firmware)\n"
          "  tbsim info                         JSON: faces, expressions, animations\n"
          "  tbsim faces <dir>                  rest pose of every candidate (.pgm .bin .txt)\n"
          "  tbsim expr <dir>                   every expression for every candidate (.pgm)\n"
          "  tbsim lab <out.pgm> <face> <expr|rest>\n"
          "  tbsim styles | params              JSON dumps of style values / parameter defs\n"
          "  tbsim anim <id> <face> <outdir> [--fps 30] [--mirror 0|1] [--tscale 1] [--seed 1]\n"
          "  tbsim reel <face> <seconds> <outdir> [--fps 30] [--seed 7]   idle + autonomous play\n"
          "  tbsim json anim <id> <face> [...] | json reel <face> <seconds> [...]\n"
          "  tbsim test [--json]                automated QA suite\n"
          "options: --set key=value | --set A:key=value  (repeatable)\n");
}

}  // namespace

int main(int argc, char** argv) {
  std::vector<std::string> a;
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--set") == 0 && i + 1 < argc) gSets.push_back(argv[++i]);
    else a.push_back(argv[i]);
  }
  if (a.empty()) { usage(); return 2; }
  const std::string& cmd = a[0];
  const size_t n = a.size();
  if (cmd == "info") return cmdInfo();
  if (cmd == "faces" && n >= 2) return cmdFaces(a[1]);
  if (cmd == "expr" && n >= 2) return cmdExpr(a[1]);
  if (cmd == "lab" && n >= 3) return cmdLab(a[1], a[2], n >= 4 ? a[3] : "rest");
  if (cmd == "styles") return cmdStyles();
  if (cmd == "params") return cmdParams();
  if (cmd == "anim" && n >= 4) return cmdAnim(a);
  if (cmd == "reel" && n >= 4) return cmdReel(a);
  if (cmd == "json" && n >= 3) return cmdJson(a);
  if (cmd == "test") return cmdTest(a);
  usage();
  return 2;
}
