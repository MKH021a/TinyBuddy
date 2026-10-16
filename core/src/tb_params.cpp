#include "tb_params.h"
#include "tb_anim.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace tb {

namespace {

#define TB_F(field, lo, hi) {#field, ParamType::Float, (uint16_t)offsetof(FaceStyle, field), lo, hi}
#define TB_E(field, type) {#field, ParamType::type, (uint16_t)offsetof(FaceStyle, field), 0, 0}
const ParamDef kParams[] = {
  TB_E(shape, Shape),       TB_E(fill, Fill),
  TB_F(eyeW, 12, 48),       TB_F(eyeH, 12, 56),        TB_F(corner, 0, 20),     TB_F(squircleN, 2, 6),
  TB_F(spacing, 30, 80),    TB_F(eyeY, 18, 40),
  TB_F(pupilRx, 2, 20),     TB_F(pupilRy, 2, 24),      TB_F(pupilDy, -6, 8),
  TB_F(travelX, 0, 0.8f),   TB_F(travelY, 0, 0.8f),
  TB_E(highlight, Highlight), TB_F(ringThick, 1, 4),   TB_F(rim, 0, 4),
  {"irisDither", ParamType::Bool, (uint16_t)offsetof(FaceStyle, irisDither), 0, 1},
  TB_E(lidLine, LidLine),   TB_F(lidGap, 0, 3),        TB_F(lidThick, 1, 4),    TB_F(lashFlick, 0, 6),
  TB_F(seam, -0.5f, 0.8f),  TB_F(lidCurve, -0.5f, 0.8f), TB_F(seamThick, 1, 4),
  TB_F(gazeShiftX, 0, 14),  TB_F(gazeShiftY, 0, 10),   TB_F(perspective, 0, 0.25f),
  TB_F(mouthY, 40, 62),     TB_F(mouthW, 2, 30),       TB_F(mouthDepth, 2, 14), TB_F(mouthLine, 1, 3),
  TB_E(mouthFill, MouthFill), TB_F(catDepth, 0, 5), TB_F(fang, 0, 3),
  TB_E(blush, Blush),       TB_F(blushDX, 0, 20),      TB_F(blushDY, -4, 10),
  TB_F(browGap, 0, 10),     TB_F(browLen, 4, 24),
};
#undef TB_F
#undef TB_E

const char* const kShape[] = {"ellipse", "roundrect", "squircle"};
const char* const kFill[] = {"solid", "pupil", "ring"};
const char* const kHighlight[] = {"none", "dots", "sparkle", "anime"};
const char* const kLidLine[] = {"none", "lash", "lid"};
const char* const kBlush[] = {"none", "hatch", "dither", "dots"};
const char* const kMouthFill[] = {"outline", "solid"};

const char* const* enumTable(ParamType t, int& n) {
  switch (t) {
    case ParamType::Shape: n = 3; return kShape;
    case ParamType::Fill: n = 3; return kFill;
    case ParamType::Highlight: n = 4; return kHighlight;
    case ParamType::LidLine: n = 3; return kLidLine;
    case ParamType::Blush: n = 4; return kBlush;
    case ParamType::MouthFill: n = 2; return kMouthFill;
    default: n = 0; return nullptr;
  }
}

const ParamDef* find(const char* key) {
  for (const ParamDef& p : kParams)
    if (strcmp(p.name, key) == 0) return &p;
  return nullptr;
}

}  // namespace

int styleParamCount() { return (int)(sizeof(kParams) / sizeof(kParams[0])); }
const ParamDef& styleParam(int i) { return kParams[i]; }

int enumCount(ParamType t) { int n; enumTable(t, n); return n; }
const char* enumName(ParamType t, int v) {
  int n;
  const char* const* tab = enumTable(t, n);
  return (tab && v >= 0 && v < n) ? tab[v] : "?";
}

bool setStyleParam(FaceStyle& st, const char* key, const char* value) {
  if (strncmp(key, "rest.", 5) == 0) {
    Ch ch;
    if (!channelByName(key + 5, ch)) return false;
    float v = strtof(value, nullptr);
    for (int i = 0; i < kChannelCount; ++i)
      if (drivesChannel(ch, (Ch)i)) *channelPtr(st.rest, (Ch)i) = v;
    return true;
  }
  const ParamDef* p = find(key);
  if (!p) return false;
  uint8_t* base = reinterpret_cast<uint8_t*>(&st) + p->offset;
  if (p->type == ParamType::Float) {
    char* end = nullptr;
    float v = strtof(value, &end);
    if (end == value) return false;
    *reinterpret_cast<float*>(base) = v;
    return true;
  }
  if (p->type == ParamType::Bool) {
    *reinterpret_cast<bool*>(base) = (value[0] == '1' || value[0] == 't' || value[0] == 'y');
    return true;
  }
  int n;
  const char* const* tab = enumTable(p->type, n);
  for (int i = 0; i < n; ++i)
    if (strcmp(tab[i], value) == 0) { *base = (uint8_t)i; return true; }
  return false;
}

bool getStyleParam(const FaceStyle& st, const char* key, char* out, int outLen) {
  if (strncmp(key, "rest.", 5) == 0) {
    Ch ch;
    if (!channelByName(key + 5, ch) || (int)ch >= kChannelCount) return false;
    snprintf(out, outLen, "%g", (double)channelGet(st.rest, ch));
    return true;
  }
  const ParamDef* p = find(key);
  if (!p) return false;
  const uint8_t* base = reinterpret_cast<const uint8_t*>(&st) + p->offset;
  if (p->type == ParamType::Float) snprintf(out, outLen, "%g", (double)*reinterpret_cast<const float*>(base));
  else if (p->type == ParamType::Bool) snprintf(out, outLen, "%d", *reinterpret_cast<const bool*>(base) ? 1 : 0);
  else snprintf(out, outLen, "%s", enumName(p->type, *base));
  return true;
}

}  // namespace tb
