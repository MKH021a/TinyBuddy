#pragma once
#include "tb_face.h"
#include "tb_ease.h"

namespace tb {

// ---------------------------------------------------------------------------
// Channels: every animatable float in FaceState gets an id. "L"/"R" = screen side.
// Group ids (>= 64) address both eyes at once and are expanded during evaluation.
// ---------------------------------------------------------------------------
enum class Ch : uint8_t {
  GazeX, GazeY, FaceX, FaceY, Squash,
  OpenL, OpenR, HappyL, HappyR, TiltL, TiltR, CurveL, CurveR,
  ScaleWL, ScaleWR, ScaleHL, ScaleHR, PupilL, PupilR, ShineL, ShineR,
  BrowL, BrowR, BrowTiltL, BrowTiltR, LookXL, LookXR,
  BrowShow, MouthCurve, MouthOpen, MouthCat, MouthW, MouthAsym, MouthTongue, MouthRound, MouthDY,
  Blush,
  kCount,
  // ---- groups ----
  Open = 64, Happy, Tilt, Curve, ScaleW, ScaleH, Scale, Pupil, Shine, Brow, BrowTilt, LookX,
};

constexpr int kChannelCount = (int)Ch::kCount;
const char* channelName(Ch c);
bool channelByName(const char* name, Ch& out);  // real channels and groups
bool drivesChannel(Ch key, Ch real);
float* channelPtr(FaceState& f, Ch c);
inline float channelGet(const FaceState& f, Ch c) { return *channelPtr(const_cast<FaceState&>(f), c); }

// How a key's value is interpreted. Relative ops let one choreography fit every face
// (e.g. "close to 72% of your resting openness" works for wide-awake and sleepy styles).
enum class Op : uint8_t {
  Abs,   // target = v
  Mul,   // target = base * v
  Add,   // target = base + v
  Base,  // target = base (the idle pose captured when the animation started)
};

// One tween: from the channel's current value, move to the target over d ms with easing e.
struct Key {
  uint16_t t, d;
  Ch ch;
  Ease e;
  Op op;
  float v;
};

enum class Wave : uint8_t { Sine, Shake, Pulse };
// Additive oscillator (tremble, wobble, pulse). Envelope fades in/out automatically.
struct Osc {
  uint16_t t, d;
  Ch ch;
  Wave wave;
  float amp;
  uint16_t period;  // ms
};

enum class FxAnim : uint8_t { Pop, Twinkle, Rise };
enum : uint8_t {
  FX_HALO = 1,      // dark outline (use when it may overlap lit pixels)
  FX_FACE = 2,      // moves with faceX/faceY (bounces with the face)
  FX_EYE0 = 4,      // x,y are % of the left eye's half-size, relative to its centre
  FX_EYE1 = 8,      // ... right eye
};
struct FxEv {
  uint16_t t, d;
  FxKind kind;
  FxAnim anim;
  int16_t x, y;  // px, or % of eye half-size with FX_EYE*
  int8_t dx, dy; // drift over lifetime (px)
  uint8_t size;  // peak sprite size index
  uint8_t flags;
};

struct Marker {
  uint16_t t;
  const char* label;
};

struct AnimDef {
  uint8_t id;
  const char* name;
  const char* category;
  const char* summary;
  uint16_t duration;     // ms (before time scaling)
  uint8_t weight;        // autonomous selection weight
  uint16_t cooldownS;    // min seconds between autonomous plays
  bool interruptible;
  bool idleBlinks;       // allow idle blinking on top (for long holds)
  const Key* keys; uint16_t nKeys;
  const Osc* oscs; uint8_t nOscs;
  const FxEv* fx; uint8_t nFx;
  const Marker* markers; uint8_t nMarkers;
};

// Per-play variation: mirrored choreography, tempo, and effect jitter seed.
struct Variant {
  bool mirror = false;
  float timeScale = 1.f;  // 1.05 = 5% slower
  uint32_t seed = 1;
};

uint32_t scaledDuration(const AnimDef& a, const Variant& v);

// Pure function: (animation, time, variant, base pose) -> face + effects.
void evaluate(const AnimDef& a, uint32_t tMs, const Variant& v, const FaceStyle& st,
              const FaceState& base, FaceState& out, FxList& fx);

// Copies b over a with blend t (all channels). Used for safe stop/recover transitions.
void blendFaces(const FaceState& a, const FaceState& b, float t, FaceState& out);

// ---------------------------------------------------------------------------
// Expressions: named static poses (channel overrides on top of the style's rest pose).
// ---------------------------------------------------------------------------
struct ChVal {
  Ch ch;
  float v;
};
struct Expression {
  const char* name;
  const ChVal* vals;
  uint8_t n;
  uint8_t fxHearts;  // decorative hearts for static preview poses
};
int expressionCount();
const Expression& expressionAt(int i);
const Expression* expressionByName(const char* name);
void applyExpression(const Expression& e, FaceState& f);

}  // namespace tb
