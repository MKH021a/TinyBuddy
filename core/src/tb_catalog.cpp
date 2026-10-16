#include "tb_catalog.h"
#include <ctype.h>
#include <string.h>

namespace tb {

namespace {

// Authoring shorthands. Times in ms. Keys MUST be in ascending start time (tests check this).
#define ABS(t, d, ch, e, v) {t, d, Ch::ch, Ease::e, Op::Abs, v}
#define MUL(t, d, ch, e, v) {t, d, Ch::ch, Ease::e, Op::Mul, v}
#define ADD(t, d, ch, e, v) {t, d, Ch::ch, Ease::e, Op::Add, v}
#define BAS(t, d, ch, e)    {t, d, Ch::ch, Ease::e, Op::Base, 0.f}
#define COUNT(a) (uint16_t)(sizeof(a) / sizeof(a[0]))

// ===========================================================================
// #80 — TinyBuddy Signature Finale
// A tiny show: settle, double blink, look left/right with anticipation, a wind-up,
// a sparkly POP, hearts & stars, a two-bounce celebration, a winking "ting!", and home.
// ===========================================================================
const Key kFinaleKeys[] = {
  // settle — look at the viewer
  ABS(0, 240, GazeX, InOutQuad, 0.f),
  ABS(0, 240, GazeY, InOutQuad, 0.f),
  ABS(0, 240, LookX, InOutQuad, 0.f),

  // double blink: a normal one, then a quicker flutter; the face nods 1px with each
  ABS(260, 70, Open, InQuad, 0.f),
  ADD(260, 90, FaceY, OutQuad, 1.f),
  BAS(360, 120, Open, OutCubic),
  BAS(360, 160, FaceY, OutQuad),
  ABS(560, 55, Open, InQuad, 0.f),
  BAS(640, 100, Open, OutCubic),

  // look left: tiny counter-move (anticipation), fast saccade with overshoot, curious hold
  ABS(820, 90, GazeX, InOutQuad, 0.14f),
  ABS(910, 150, GazeX, OutBack, -1.f),
  ABS(910, 150, GazeY, OutQuad, -0.08f),
  MUL(910, 200, Open, OutQuad, 1.06f),
  ABS(1250, 140, GazeX, OutQuad, -0.82f),            // "reading" micro-adjust

  // look right: the long crossing saccade carries a half blink
  ABS(1500, 210, GazeX, InOutCubic, 1.f),
  MUL(1515, 60, Open, InQuad, 0.35f),
  MUL(1575, 120, Open, OutCubic, 1.06f),
  ABS(1860, 130, GazeX, OutQuad, 0.84f),

  // back to centre
  ABS(2050, 170, GazeX, OutQuart, 0.f),
  ABS(2050, 170, GazeY, OutQuart, 0.f),

  // wind-up: squint, compress, dip, purse the mouth
  MUL(2200, 220, Open, InOutQuad, 0.72f),
  ABS(2200, 220, Squash, InOutQuad, 0.10f),
  ABS(2200, 220, FaceY, InOutQuad, 2.f),
  MUL(2200, 220, MouthW, InOutQuad, 0.7f),

  // POP: eyes fly open and grow, pupils dilate, catch-lights flare, big open smile, blush
  ABS(2420, 200, Open, OutBack, 1.f),
  ABS(2420, 160, Happy, OutQuad, 0.f),
  ABS(2420, 260, ScaleW, OutBackBig, 1.10f),
  ABS(2420, 260, ScaleH, OutBackBig, 1.18f),
  ABS(2420, 160, Squash, OutQuad, -0.08f),
  ABS(2420, 200, FaceY, OutBack, -2.f),
  ABS(2420, 220, Pupil, OutBackBig, 1.15f),
  ABS(2420, 220, Shine, OutBack, 1.45f),
  ABS(2420, 160, MouthCat, OutQuad, 0.f),
  ABS(2420, 160, MouthCurve, OutQuad, 2.2f),
  ABS(2420, 160, MouthOpen, OutBack, 0.75f),
  ABS(2420, 160, MouthRound, OutQuad, 0.35f),
  ABS(2420, 160, MouthW, OutBack, 1.2f),
  ABS(2420, 300, Blush, OutQuad, 1.f),
  ABS(2580, 140, Squash, OutQuad, 0.f),

  // celebrate: eyes turn into happy arches, laughing mouth, two bounces with squash & stretch
  ABS(2700, 160, Happy, OutQuad, 1.f),
  ABS(2700, 220, ScaleW, OutQuad, 1.04f),
  ABS(2700, 220, ScaleH, OutQuad, 1.04f),
  ABS(2700, 150, MouthOpen, OutQuad, 0.9f),
  ABS(2700, 150, MouthRound, OutQuad, 0.f),
  ABS(2700, 150, MouthTongue, OutQuad, 0.8f),
  ABS(2700, 150, MouthCurve, OutQuad, 2.f),
  ABS(2720, 110, Squash, OutQuad, 0.14f),            // crouch
  ABS(2720, 110, FaceY, OutQuad, 2.f),
  ABS(2830, 170, Squash, OutQuad, -0.10f),           // leap (stretch)
  ABS(2830, 170, FaceY, OutCubic, -5.f),
  ABS(3000, 150, FaceY, InQuad, 1.f),                // fall
  ABS(3000, 150, Squash, InQuad, 0.f),
  ABS(3150, 90, Squash, OutQuad, 0.16f),             // land
  ABS(3150, 90, FaceY, OutQuad, 2.f),
  ABS(3240, 140, Squash, OutQuad, -0.08f),           // smaller second hop
  ABS(3240, 140, FaceY, OutCubic, -3.f),
  ABS(3380, 130, FaceY, InQuad, 1.f),
  ABS(3380, 130, Squash, InQuad, 0.f),
  ABS(3510, 80, Squash, OutQuad, 0.08f),
  ABS(3510, 80, FaceY, OutQuad, 1.f),
  ABS(3590, 260, Squash, OutElastic, 0.f),
  BAS(3590, 220, FaceY, OutBack),

  // settle into a warm smile with sparkly eyes
  ABS(3600, 280, Happy, InOutQuad, 0.3f),
  ABS(3600, 280, ScaleW, InOutQuad, 1.f),
  ABS(3600, 280, ScaleH, InOutQuad, 1.f),
  ABS(3600, 280, Pupil, InOutQuad, 1.08f),
  ABS(3600, 280, Shine, InOutQuad, 1.2f),
  ABS(3600, 240, MouthOpen, InOutQuad, 0.f),
  ABS(3600, 240, MouthTongue, InOutQuad, 0.f),
  ABS(3600, 240, MouthW, InOutQuad, 1.1f),

  // lean in ... and WINK (right eye), with a cheeky tongue and a little hop
  ABS(3950, 220, FaceX, InOutQuad, 2.f),
  ABS(3950, 220, GazeX, InOutQuad, -0.12f),
  ABS(4170, 80, OpenR, OutQuad, 0.f),                // the lid snaps shut first ...
  ABS(4170, 120, OpenL, OutQuad, 1.f),
  ABS(4170, 120, HappyL, OutQuad, 0.3f),
  ABS(4170, 140, MouthAsym, OutBack, -1.4f),
  ABS(4170, 140, MouthCurve, OutBack, 2.4f),
  ABS(4170, 140, MouthOpen, OutBack, 0.38f),
  ABS(4170, 140, MouthTongue, OutBack, 1.f),
  ADD(4170, 120, FaceY, OutQuad, -1.f),
  ABS(4240, 110, HappyR, InOutQuad, 1.f),            // ... then the closed line bends into "^"
  ABS(4240, 110, OpenR, InOutQuad, 0.12f),
  BAS(4290, 200, FaceY, OutBack),

  // unwink: the "^" flattens back into a closed line, then the eye opens
  ABS(4850, 70, OpenR, InQuad, 0.f),
  ABS(4850, 170, MouthTongue, InOutQuad, 0.f),
  ABS(4850, 170, MouthOpen, InOutQuad, 0.f),
  ABS(4900, 100, HappyR, InOutQuad, 0.3f),
  ABS(4990, 200, OpenR, OutCubic, 1.f),

  // recover to the resting face
  BAS(5000, 320, Happy, InOutQuad),
  BAS(5000, 320, Pupil, InOutQuad),
  BAS(5000, 320, Shine, InOutQuad),
  BAS(5000, 300, FaceX, InOutQuad),
  BAS(5000, 300, GazeX, InOutQuad),
  BAS(5000, 300, GazeY, InOutQuad),
  BAS(5000, 300, LookX, InOutQuad),
  BAS(5000, 300, MouthAsym, InOutQuad),
  BAS(5000, 300, MouthCurve, InOutQuad),
  BAS(5000, 300, MouthW, InOutQuad),
  BAS(5000, 300, MouthCat, InOutQuad),
  BAS(5000, 300, MouthRound, InOutQuad),
  BAS(5000, 300, MouthOpen, InOutQuad),
  BAS(5000, 500, Blush, InOutSine),
  BAS(5000, 300, Squash, InOutQuad),
  BAS(5200, 260, Open, InOutQuad),

  // one last soft blink to close the show
  ABS(5500, 80, Open, InQuad, 0.f),
  BAS(5600, 170, Open, OutCubic),
};

const FxEv kFinaleFx[] = {
  // hearts pop at the outer corners and float up with a heartbeat
  {2480, 1350, FxKind::Heart, FxAnim::Pop, -118, -78, -2, -6, 4, FX_HALO | FX_EYE0},
  {2560, 1300, FxKind::Heart, FxAnim::Pop, 118, -70, 2, -6, 3, FX_HALO | FX_EYE1},
  // sparkles twinkle in the free corners of the screen
  {2500, 520, FxKind::Sparkle, FxAnim::Twinkle, 64, 7, 0, 0, 4, FX_HALO | FX_FACE},
  {2700, 480, FxKind::Sparkle, FxAnim::Twinkle, 9, 52, 0, 0, 3, FX_HALO | FX_FACE},
  {2860, 520, FxKind::Sparkle, FxAnim::Twinkle, 118, 50, 0, 0, 4, FX_HALO | FX_FACE},
  {3120, 460, FxKind::Sparkle, FxAnim::Twinkle, 8, 14, 0, 0, 3, FX_HALO | FX_FACE},
  {3330, 440, FxKind::Sparkle, FxAnim::Twinkle, 64, 7, 0, 0, 3, FX_HALO | FX_FACE},
  // the wink "ting!"
  {4200, 620, FxKind::Sparkle, FxAnim::Twinkle, 96, -92, 2, -2, 4, FX_HALO | FX_EYE1},
};

const Marker kFinaleMarks[] = {
  {0, "settle"},          {260, "double blink"}, {820, "look left"},   {1500, "look right"},
  {2050, "centre"},       {2200, "wind-up"},     {2420, "POP!"},       {2700, "celebrate x2"},
  {3600, "warm smile"},   {3950, "lean in"},     {4170, "wink + ting"}, {4850, "unwink"},
  {5000, "recover"},      {5500, "soft blink"},  {5800, "idle"},
};

const AnimDef kCatalog[] = {
  {80, "TinyBuddy Signature Finale", "Silly & playful",
   "Double blink, look left & right, wind-up, sparkly pop with hearts & stars, two-bounce "
   "celebration, winking 'ting!', back to idle.",
   5800, 3, 90, true, false,
   kFinaleKeys, COUNT(kFinaleKeys), nullptr, 0, kFinaleFx, (uint8_t)COUNT(kFinaleFx),
   kFinaleMarks, (uint8_t)COUNT(kFinaleMarks)},
};

bool ieq(const char* a, const char* b) {
  for (; *a && *b; ++a, ++b)
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
  return *a == *b;
}

bool icontains(const char* hay, const char* needle) {
  size_t n = strlen(needle);
  if (!n) return false;
  for (const char* h = hay; *h; ++h) {
    size_t i = 0;
    while (i < n && h[i] && tolower((unsigned char)h[i]) == tolower((unsigned char)needle[i])) ++i;
    if (i == n) return true;
  }
  return false;
}

}  // namespace

int animCount() { return (int)(sizeof(kCatalog) / sizeof(kCatalog[0])); }
const AnimDef& animAt(int index) { return kCatalog[index]; }

const AnimDef* animById(int id) {
  for (const AnimDef& a : kCatalog)
    if (a.id == id) return &a;
  return nullptr;
}

const AnimDef* animByName(const char* name) {
  for (const AnimDef& a : kCatalog)
    if (ieq(a.name, name)) return &a;
  const AnimDef* hit = nullptr;
  for (const AnimDef& a : kCatalog)
    if (icontains(a.name, name)) {
      if (hit) return nullptr;  // ambiguous
      hit = &a;
    }
  return hit;
}

}  // namespace tb
