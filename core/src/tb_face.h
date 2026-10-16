#pragma once
#include "tb_types.h"

namespace tb {

// ---------------------------------------------------------------------------
// FaceState: every animatable parameter of the face for one frame.
// Animations never draw; they only produce a FaceState (+ effects) for time t.
// Index 0 = the eye on the LEFT of the screen, 1 = the RIGHT one.
// ---------------------------------------------------------------------------
struct EyeState {
  float open = 1.f;      // lid aperture: 1 = fully open, 0 = closed seam
  float happy = 0.f;     // cheek push from below: 1 = "^" arch eye
  float tilt = 0.f;      // top-lid slope; + = angry (inner corner low), - = sad (outer low)
  float curve = 0.f;     // extra lid curvature added on top of the style's natural curve
  float scaleW = 1.f;    // eye size multipliers
  float scaleH = 1.f;
  float pupil = 1.f;     // pupil size multiplier
  float shine = 1.f;     // catch-light size multiplier
  float dx = 0.f, dy = 0.f;  // per-eye nudge (px)
  float lookX = 0.f;     // per-eye pupil offset added to gaze (cross-eye, wall-eye)
  float brow = 0.f;      // brow lift (px, + = up)
  float browTilt = 0.f;  // + = angry (inner end down)
};

struct FaceState {
  EyeState eye[2];
  float gazeX = 0.f, gazeY = 0.f;  // -1..1 (left/up negative)
  float faceX = 0.f, faceY = 0.f;  // whole-face offset (px)
  float squash = 0.f;              // + = squash (wide/short), - = stretch (tall/thin)
  // Mouth: a parametric family that morphs continuously between all presets.
  float mouthCurve = 0.f;   // px; + smile (centre lower), - frown
  float mouthOpen = 0.f;    // 0 closed line .. 1 fully open
  float mouthCat = 0.f;     // 0 plain .. 1 cat "w" top lip
  float mouthW = 1.f;       // width multiplier
  float mouthAsym = 0.f;    // px; tilts the mouth for smirks
  float mouthTongue = 0.f;  // 0..1
  float mouthRound = 0.f;   // 0 = "D" open smile, 1 = round "o"
  float mouthDY = 0.f;      // px
  float blush = 0.f;        // 0..1
  float browShow = 0.f;     // 0..1 (drawn when > 0.5)
};

// ---------------------------------------------------------------------------
// Effects (hearts, sparkles, ...) — evaluated statelessly per frame.
// ---------------------------------------------------------------------------
enum class FxKind : uint8_t { Heart, Sparkle, Dot, Count };

struct FxSprite {
  FxKind kind;
  int16_t x, y;   // centre (px)
  uint8_t size;   // sprite size index (0 = hidden)
  bool halo;      // 1px dark outline so it reads on top of lit eyes
};

struct FxList {
  static constexpr int kMax = 16;
  FxSprite item[kMax];
  uint8_t n = 0;
  void clear() { n = 0; }
  void add(const FxSprite& s) { if (n < kMax) item[n++] = s; }
};

// ---------------------------------------------------------------------------
// FaceStyle: the static design of a face (shape language, proportions, rest pose).
// The six candidates are six FaceStyles driven by the very same FaceState/animations.
// ---------------------------------------------------------------------------
enum class EyeShape : uint8_t { Ellipse, RoundRect, Squircle };
enum class EyeFill : uint8_t { Solid, Pupil, Ring };
enum class Highlight : uint8_t { None, Dots, Sparkle, Anime };
enum class LidLine : uint8_t { None, Lash, Lid };
enum class Blush : uint8_t { None, Hatch, Dither, Dots };
enum class MouthFill : uint8_t { Outline, Solid };

struct FaceStyle {
  char id;
  const char* name;
  const char* motto;
  // eye shape
  EyeShape shape;
  EyeFill fill;
  float eyeW, eyeH;     // px (full size)
  float corner;         // RoundRect corner radius (px)
  float squircleN;      // Squircle exponent (2 = ellipse, 4 = squarish)
  float spacing;        // distance between eye centres (px)
  float eyeY;           // eye centre y (px)
  // pupils + highlights
  float pupilRx, pupilRy, pupilDy;  // px
  float travelX, travelY;           // fraction of free room the pupil may use for gaze
  Highlight highlight;
  float ringThick;      // Ring fill outline thickness
  float rim;            // lit rim kept between pupil and eye outline / lid edge (px)
  bool irisDither;      // dithered glow in the lower iris (anime look)
  // lids
  LidLine lidLine;
  float lidGap, lidThick, lashFlick;
  float seam;           // where the lids meet, as a fraction of half-height below centre
  float lidCurve;       // natural arch of the upper lid
  float seamThick;      // closed-eye line thickness (px)
  // gaze response
  float gazeShiftX, gazeShiftY;  // eyes slide with gaze (px)
  float perspective;             // near-eye grows when looking sideways
  // mouth
  float mouthY, mouthW, mouthDepth, mouthLine;
  MouthFill mouthFill;
  float catDepth;
  float fang;           // 0 = none, 2-3 = little fang pixels under the lip
  // cheeks + brows
  Blush blush;
  float blushDX, blushDY;
  float browGap, browLen;
  // the pose this face rests in
  FaceState rest;
};

int styleCount();
const FaceStyle& styleAt(int index);
const FaceStyle* styleById(char id);

}  // namespace tb
