#include "tb_anim.h"
#include "tb_render.h"
#include "tb_rng.h"
#include "tb_sprites.h"
#include <string.h>

namespace tb {

namespace {

const char* const kChNames[kChannelCount] = {
  "gazeX", "gazeY", "faceX", "faceY", "squash",
  "openL", "openR", "happyL", "happyR", "tiltL", "tiltR", "curveL", "curveR",
  "scaleWL", "scaleWR", "scaleHL", "scaleHR", "pupilL", "pupilR", "shineL", "shineR",
  "browL", "browR", "browTiltL", "browTiltR", "lookXL", "lookXR",
  "browShow", "mouthCurve", "mouthOpen", "mouthCat", "mouthW", "mouthAsym", "mouthTongue",
  "mouthRound", "mouthDY", "blush",
};

bool isGroup(Ch c) { return (uint8_t)c >= 64; }

// Does a key addressed to `k` drive real channel `c`?
bool drives(Ch k, Ch c) {
  if (k == c) return true;
  if (!isGroup(k)) return false;
  switch (k) {
    case Ch::Open: return c == Ch::OpenL || c == Ch::OpenR;
    case Ch::Happy: return c == Ch::HappyL || c == Ch::HappyR;
    case Ch::Tilt: return c == Ch::TiltL || c == Ch::TiltR;
    case Ch::Curve: return c == Ch::CurveL || c == Ch::CurveR;
    case Ch::ScaleW: return c == Ch::ScaleWL || c == Ch::ScaleWR;
    case Ch::ScaleH: return c == Ch::ScaleHL || c == Ch::ScaleHR;
    case Ch::Scale: return c == Ch::ScaleWL || c == Ch::ScaleWR || c == Ch::ScaleHL || c == Ch::ScaleHR;
    case Ch::Pupil: return c == Ch::PupilL || c == Ch::PupilR;
    case Ch::Shine: return c == Ch::ShineL || c == Ch::ShineR;
    case Ch::Brow: return c == Ch::BrowL || c == Ch::BrowR;
    case Ch::BrowTilt: return c == Ch::BrowTiltL || c == Ch::BrowTiltR;
    case Ch::LookX: return c == Ch::LookXL || c == Ch::LookXR;
    default: return false;
  }
}

// Mirroring swaps left/right eye channels; groups are symmetric already.
Ch mirrorCh(Ch c) {
  switch (c) {
    case Ch::OpenL: return Ch::OpenR;       case Ch::OpenR: return Ch::OpenL;
    case Ch::HappyL: return Ch::HappyR;     case Ch::HappyR: return Ch::HappyL;
    case Ch::TiltL: return Ch::TiltR;       case Ch::TiltR: return Ch::TiltL;
    case Ch::CurveL: return Ch::CurveR;     case Ch::CurveR: return Ch::CurveL;
    case Ch::ScaleWL: return Ch::ScaleWR;   case Ch::ScaleWR: return Ch::ScaleWL;
    case Ch::ScaleHL: return Ch::ScaleHR;   case Ch::ScaleHR: return Ch::ScaleHL;
    case Ch::PupilL: return Ch::PupilR;     case Ch::PupilR: return Ch::PupilL;
    case Ch::ShineL: return Ch::ShineR;     case Ch::ShineR: return Ch::ShineL;
    case Ch::BrowL: return Ch::BrowR;       case Ch::BrowR: return Ch::BrowL;
    case Ch::BrowTiltL: return Ch::BrowTiltR; case Ch::BrowTiltR: return Ch::BrowTiltL;
    case Ch::LookXL: return Ch::LookXR;     case Ch::LookXR: return Ch::LookXL;
    default: return c;
  }
}

bool negatesOnMirror(Ch c) {
  return c == Ch::GazeX || c == Ch::FaceX || c == Ch::MouthAsym || c == Ch::LookXL || c == Ch::LookXR ||
         c == Ch::LookX;
}

const char* const kGroupNames[] = {"open", "happy", "tilt", "curve", "scaleW", "scaleH", "scale",
                                   "pupil", "shine", "brow", "browTilt", "lookX"};

float keyTarget(const Key& k, Ch c, float baseV, bool mirror) {
  float v = (mirror && negatesOnMirror(c)) ? -k.v : k.v;
  switch (k.op) {
    case Op::Abs: return v;
    case Op::Mul: return baseV * k.v;
    case Op::Add: return baseV + v;
    case Op::Base: return baseV;
  }
  return v;
}

float segValue(const Key& k, float kt, float kd, float from, float target, float t) {
  if (kd <= 0.f) return t >= kt ? target : from;
  return lerpf(from, target, ease(k.e, (t - kt) / kd));
}

float oscValue(const Osc& o, float t0, float dur, float t, uint32_t seed) {
  float p = (t - t0) / dur;
  if (p < 0.f || p > 1.f) return 0.f;
  float env = fminf2(1.f, p / 0.12f) * fminf2(1.f, (1.f - p) / 0.4f);  // attack, long release
  float ph = (t - t0) / (float)o.period;
  switch (o.wave) {
    case Wave::Sine: return o.amp * env * sinf(2.f * kPi * ph);
    case Wave::Pulse: return o.amp * env * (0.5f - 0.5f * cosf(2.f * kPi * ph));
    case Wave::Shake: {
      uint32_t step = (uint32_t)ph;
      float a = hash01(seed, step) * 2.f - 1.f;
      float b = hash01(seed, step + 1) * 2.f - 1.f;
      float f = ph - (float)step;
      return o.amp * env * lerpf(a, b, f);
    }
  }
  return 0.f;
}

}  // namespace

const char* channelName(Ch c) {
  int i = (int)c;
  if (i < kChannelCount) return kChNames[i];
  int g = i - 64;
  return (g >= 0 && g < (int)(sizeof(kGroupNames) / sizeof(kGroupNames[0]))) ? kGroupNames[g] : "?";
}

bool channelByName(const char* name, Ch& out) {
  for (int i = 0; i < kChannelCount; ++i)
    if (strcmp(kChNames[i], name) == 0) { out = (Ch)i; return true; }
  for (int g = 0; g < (int)(sizeof(kGroupNames) / sizeof(kGroupNames[0])); ++g)
    if (strcmp(kGroupNames[g], name) == 0) { out = (Ch)(64 + g); return true; }
  return false;
}

bool drivesChannel(Ch key, Ch real) { return drives(key, real); }

float* channelPtr(FaceState& f, Ch c) {
  switch (c) {
    case Ch::GazeX: return &f.gazeX;
    case Ch::GazeY: return &f.gazeY;
    case Ch::FaceX: return &f.faceX;
    case Ch::FaceY: return &f.faceY;
    case Ch::Squash: return &f.squash;
    case Ch::OpenL: return &f.eye[0].open;
    case Ch::OpenR: return &f.eye[1].open;
    case Ch::HappyL: return &f.eye[0].happy;
    case Ch::HappyR: return &f.eye[1].happy;
    case Ch::TiltL: return &f.eye[0].tilt;
    case Ch::TiltR: return &f.eye[1].tilt;
    case Ch::CurveL: return &f.eye[0].curve;
    case Ch::CurveR: return &f.eye[1].curve;
    case Ch::ScaleWL: return &f.eye[0].scaleW;
    case Ch::ScaleWR: return &f.eye[1].scaleW;
    case Ch::ScaleHL: return &f.eye[0].scaleH;
    case Ch::ScaleHR: return &f.eye[1].scaleH;
    case Ch::PupilL: return &f.eye[0].pupil;
    case Ch::PupilR: return &f.eye[1].pupil;
    case Ch::ShineL: return &f.eye[0].shine;
    case Ch::ShineR: return &f.eye[1].shine;
    case Ch::BrowL: return &f.eye[0].brow;
    case Ch::BrowR: return &f.eye[1].brow;
    case Ch::BrowTiltL: return &f.eye[0].browTilt;
    case Ch::BrowTiltR: return &f.eye[1].browTilt;
    case Ch::LookXL: return &f.eye[0].lookX;
    case Ch::LookXR: return &f.eye[1].lookX;
    case Ch::BrowShow: return &f.browShow;
    case Ch::MouthCurve: return &f.mouthCurve;
    case Ch::MouthOpen: return &f.mouthOpen;
    case Ch::MouthCat: return &f.mouthCat;
    case Ch::MouthW: return &f.mouthW;
    case Ch::MouthAsym: return &f.mouthAsym;
    case Ch::MouthTongue: return &f.mouthTongue;
    case Ch::MouthRound: return &f.mouthRound;
    case Ch::MouthDY: return &f.mouthDY;
    case Ch::Blush: return &f.blush;
    default: {
      static float sink;  // groups never reach here; harmless fallback
      return &sink;
    }
  }
}

uint32_t scaledDuration(const AnimDef& a, const Variant& v) {
  return (uint32_t)(a.duration * v.timeScale + 0.5f);
}

void evaluate(const AnimDef& a, uint32_t tMs, const Variant& var, const FaceStyle& st,
              const FaceState& base, FaceState& out, FxList& fx) {
  out = base;
  const float t = (float)tMs;
  const float ts = var.timeScale;

  for (int ci = 0; ci < kChannelCount; ++ci) {
    Ch c = (Ch)ci;
    float baseV = channelGet(base, c);
    const Key* prev = nullptr;
    float prevFrom = baseV, prevKt = 0.f, prevKd = 0.f;
    for (uint16_t i = 0; i < a.nKeys; ++i) {
      const Key& k = a.keys[i];
      Ch kc = var.mirror ? mirrorCh(k.ch) : k.ch;
      if (!drives(kc, c)) continue;
      float kt = k.t * ts, kd = k.d * ts;
      if (kt > t) break;  // keys are authored in time order (verified by tests)
      float from = prev ? segValue(*prev, prevKt, prevKd, prevFrom, keyTarget(*prev, c, baseV, var.mirror), kt)
                        : baseV;
      prev = &k; prevFrom = from; prevKt = kt; prevKd = kd;
    }
    float v = prev ? segValue(*prev, prevKt, prevKd, prevFrom, keyTarget(*prev, c, baseV, var.mirror), t) : baseV;
    for (uint8_t i = 0; i < a.nOscs; ++i) {
      const Osc& o = a.oscs[i];
      Ch oc = var.mirror ? mirrorCh(o.ch) : o.ch;
      if (!drives(oc, c)) continue;
      float ov = oscValue(o, o.t * ts, o.d * ts, t, var.seed * 31u + i);
      v += (var.mirror && negatesOnMirror(c)) ? -ov : ov;
    }
    *channelPtr(out, c) = v;
  }

  // ---- effects ----
  fx.clear();
  for (uint8_t i = 0; i < a.nFx; ++i) {
    const FxEv& e = a.fx[i];
    float t0 = e.t * ts, d = e.d * ts;
    if (t < t0 || t > t0 + d || d <= 0.f) continue;
    float p = (t - t0) / d;
    float s = 0.f;
    switch (e.anim) {
      case FxAnim::Pop: {
        float in = clampf(p / 0.16f, 0.f, 1.f), outp = clampf((1.f - p) / 0.16f, 0.f, 1.f);
        s = e.size * fminf2(ease(Ease::OutBackBig, in), ease(Ease::OutQuad, outp));
        // heartbeat: a quick double pulse each 420 ms while held
        float beat = fmodf(t - t0, 420.f * ts);
        if (in >= 1.f && outp >= 1.f && (beat < 70.f * ts || (beat > 140.f * ts && beat < 200.f * ts))) s += 1.f;
        break;
      }
      case FxAnim::Twinkle: s = e.size * sinf(kPi * p); break;
      case FxAnim::Rise: s = e.size * fminf2(1.f, fminf2(p / 0.2f, (1.f - p) / 0.25f)); break;
    }
    int size = (int)(s + 0.5f);
    if (size <= 0) continue;
    float jx = (hash01(var.seed, i * 2u) - 0.5f) * 3.f;
    float jy = (hash01(var.seed, i * 2u + 1u) - 0.5f) * 2.f;
    float drift = ease(Ease::OutCubic, p);
    float x, y;
    int8_t dx = e.dx;
    if (e.flags & (FX_EYE0 | FX_EYE1)) {
      int eye = (e.flags & FX_EYE0) ? 0 : 1;
      float ex = e.x;
      if (var.mirror) { eye = 1 - eye; ex = -ex; }
      EyeBox b = eyeBox(st, out, eye);
      x = b.cx + ex * 0.01f * b.a;
      y = b.cy + e.y * 0.01f * b.b;
    } else {
      x = var.mirror ? (float)(kW - 1 - e.x) : (float)e.x;
      y = e.y;
      if (e.flags & FX_FACE) { x += out.faceX; y += out.faceY; }
    }
    if (var.mirror) dx = (int8_t)-dx;
    x += jx + dx * drift;
    y += jy + e.dy * drift;
    FxSprite sp{e.kind, (int16_t)floorf(x + 0.5f), (int16_t)floorf(y + 0.5f), (uint8_t)size,
                (e.flags & FX_HALO) != 0};
    fx.add(sp);
  }
}

void blendFaces(const FaceState& a, const FaceState& b, float t, FaceState& out) {
  FaceState r = a;
  for (int ci = 0; ci < kChannelCount; ++ci) {
    Ch c = (Ch)ci;
    *channelPtr(r, c) = lerpf(channelGet(a, c), channelGet(b, c), t);
  }
  out = r;
}

// ---------------------------------------------------------------------------
// Expression library (static poses). Used by the preview sheets, and by
// animations as reference values when authoring.
// ---------------------------------------------------------------------------
namespace {
const ChVal kNeutral[] = {{Ch::GazeX, 0.f}};
const ChVal kBlink[] = {{Ch::Open, 0.f}};
const ChVal kHappy[] = {{Ch::Open, 1.f}, {Ch::Happy, 0.45f}, {Ch::MouthCurve, 2.5f}, {Ch::MouthCat, 0.f},
                        {Ch::MouthW, 1.15f}, {Ch::Blush, 1.f}};
const ChVal kJoy[] = {{Ch::Happy, 1.f}, {Ch::Open, 1.f}, {Ch::MouthCurve, 2.f}, {Ch::MouthOpen, 0.9f},
                      {Ch::MouthCat, 0.f}, {Ch::MouthW, 1.25f}, {Ch::MouthTongue, 0.8f}, {Ch::Blush, 1.f}};
const ChVal kSurprised[] = {{Ch::Open, 1.f}, {Ch::Happy, 0.f}, {Ch::ScaleW, 1.08f}, {Ch::ScaleH, 1.16f}, {Ch::Pupil, 0.72f},
                            {Ch::MouthOpen, 0.9f}, {Ch::MouthRound, 1.f}, {Ch::MouthW, 0.55f},
                            {Ch::MouthCat, 0.f}, {Ch::MouthCurve, 0.f}, {Ch::BrowShow, 1.f},
                            {Ch::Brow, 3.f}, {Ch::BrowTilt, -0.15f}, {Ch::FaceY, -1.f}};
const ChVal kSad[] = {{Ch::Open, 0.78f}, {Ch::Tilt, -0.42f}, {Ch::GazeY, 0.35f}, {Ch::Pupil, 1.05f},
                      {Ch::MouthCurve, -2.2f}, {Ch::MouthCat, 0.f}, {Ch::MouthW, 0.8f},
                      {Ch::BrowShow, 1.f}, {Ch::BrowTilt, -0.45f}, {Ch::Brow, 1.f}, {Ch::FaceY, 1.f}};
const ChVal kAngry[] = {{Ch::Open, 0.82f}, {Ch::Tilt, 0.55f}, {Ch::Pupil, 0.85f}, {Ch::MouthCurve, -1.6f},
                        {Ch::MouthCat, 0.f}, {Ch::MouthW, 0.9f}, {Ch::BrowShow, 1.f},
                        {Ch::BrowTilt, 0.55f}, {Ch::Brow, -1.f}};
const ChVal kSleepy[] = {{Ch::Open, 0.32f}, {Ch::GazeY, 0.45f}, {Ch::Curve, -0.25f}, {Ch::MouthOpen, 0.35f},
                         {Ch::MouthRound, 1.f}, {Ch::MouthW, 0.4f}, {Ch::MouthCat, 0.f}, {Ch::FaceY, 1.f}};
const ChVal kLove[] = {{Ch::Open, 1.f}, {Ch::Happy, 0.35f}, {Ch::Pupil, 1.12f}, {Ch::Shine, 1.35f}, {Ch::MouthCurve, 2.f},
                       {Ch::MouthCat, 1.f}, {Ch::Blush, 1.f}};
const ChVal kWink[] = {{Ch::OpenR, 0.12f}, {Ch::HappyR, 1.f}, {Ch::HappyL, 0.3f}, {Ch::MouthCurve, 2.2f},
                       {Ch::MouthAsym, -1.2f}, {Ch::MouthCat, 0.f}, {Ch::MouthOpen, 0.45f},
                       {Ch::MouthTongue, 1.f}, {Ch::Blush, 1.f}};
const ChVal kLookLeft[] = {{Ch::GazeX, -1.f}, {Ch::GazeY, -0.1f}, {Ch::LookX, 0.f}};
const ChVal kLookUp[] = {{Ch::GazeY, -1.f}, {Ch::GazeX, 0.35f}, {Ch::BrowShow, 1.f}, {Ch::Brow, 2.f}};
const ChVal kSmug[] = {{Ch::Open, 0.6f}, {Ch::Curve, -0.2f}, {Ch::Happy, 0.3f}, {Ch::GazeX, 0.7f},
                       {Ch::MouthCurve, 1.5f}, {Ch::MouthAsym, 1.6f}, {Ch::MouthCat, 0.f}};

#define TB_EXPR(name, arr, hearts) {name, arr, (uint8_t)(sizeof(arr) / sizeof(arr[0])), hearts}
const Expression kExpressions[] = {
  TB_EXPR("neutral", kNeutral, 0),   TB_EXPR("blink", kBlink, 0),
  TB_EXPR("happy", kHappy, 0),       TB_EXPR("joy", kJoy, 0),
  TB_EXPR("surprised", kSurprised, 0), TB_EXPR("sad", kSad, 0),
  TB_EXPR("angry", kAngry, 0),       TB_EXPR("sleepy", kSleepy, 0),
  TB_EXPR("love", kLove, 1),         TB_EXPR("wink", kWink, 0),
  TB_EXPR("look_left", kLookLeft, 0), TB_EXPR("look_up", kLookUp, 0),
  TB_EXPR("smug", kSmug, 0),
};
#undef TB_EXPR
}  // namespace

int expressionCount() { return (int)(sizeof(kExpressions) / sizeof(kExpressions[0])); }
const Expression& expressionAt(int i) { return kExpressions[i]; }
const Expression* expressionByName(const char* name) {
  for (int i = 0; i < expressionCount(); ++i)
    if (strcmp(kExpressions[i].name, name) == 0) return &kExpressions[i];
  return nullptr;
}

void applyExpression(const Expression& e, FaceState& f) {
  for (uint8_t i = 0; i < e.n; ++i) {
    const ChVal& cv = e.vals[i];
    for (int ci = 0; ci < kChannelCount; ++ci)
      if (drives(cv.ch, (Ch)ci)) *channelPtr(f, (Ch)ci) = cv.v;
  }
}

}  // namespace tb
