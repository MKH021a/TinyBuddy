// The six TinyBuddy face candidates. Each is a FaceStyle: the same FaceState/animation
// drives all of them, so any candidate can perform any animation.
#include "tb_face.h"

namespace tb {

namespace {

FaceStyle baseStyle() {
  FaceStyle s{};
  s.shape = EyeShape::Ellipse;
  s.fill = EyeFill::Pupil;
  s.eyeW = 30; s.eyeH = 36; s.corner = 8; s.squircleN = 3.f;
  s.spacing = 52; s.eyeY = 28;
  s.pupilRx = 10; s.pupilRy = 10; s.pupilDy = 1.5f;
  s.travelX = 0.40f; s.travelY = 0.30f;
  s.highlight = Highlight::Dots;
  s.ringThick = 2; s.rim = 1; s.irisDither = false;
  s.lidLine = LidLine::None; s.lidGap = 1; s.lidThick = 2; s.lashFlick = 0;
  s.seam = 0.30f; s.lidCurve = 0.25f; s.seamThick = 2.0f;
  s.gazeShiftX = 3; s.gazeShiftY = 2; s.perspective = 0.04f;
  s.mouthY = 52; s.mouthW = 10; s.mouthDepth = 6; s.mouthLine = 1; s.mouthFill = MouthFill::Outline;
  s.catDepth = 2;
  s.blush = Blush::Hatch; s.blushDX = 8; s.blushDY = 3;
  s.browGap = 4; s.browLen = 14;
  s.rest = FaceState{};
  s.rest.mouthCurve = 1.f;
  return s;
}

// A — Soft & Round: big round eyes, glossy dot catch-lights, cat mouth, shy hatch blush.
FaceStyle styleA() {
  FaceStyle s = baseStyle();
  s.id = 'A'; s.name = "Mochi"; s.motto = "soft & round";
  s.eyeW = 30; s.eyeH = 34; s.eyeY = 27; s.spacing = 56;
  s.pupilRx = 13.5f; s.pupilRy = 14.5f; s.pupilDy = 1.5f;   // r2: big glossy iris (r1 10.5 read as "googly")
  s.mouthW = 12;
  s.rest.eye[0].lookX = 0.08f; s.rest.eye[1].lookX = -0.08f;  // r2: slight convergence = "looking at you"
  s.rest.mouthCat = 1.f; s.rest.mouthCurve = 0.3f;
  s.rest.blush = 1.f;
  return s;
}

// B — Tiny Anime Companion: tall eyes, huge iris with dithered glow, bold lash with flick.
FaceStyle styleB() {
  FaceStyle s = baseStyle();
  s.id = 'B'; s.name = "Kira"; s.motto = "tiny anime companion";
  s.eyeW = 30; s.eyeH = 36; s.spacing = 54; s.eyeY = 29;     // r2: rounder (r1 26x40 read "alien")
  s.pupilRx = 12.5f; s.pupilRy = 15; s.pupilDy = 2.5f;
  s.highlight = Highlight::Anime; s.irisDither = true;
  s.lidLine = LidLine::Lash; s.lidThick = 2; s.lidGap = 0; s.lashFlick = 2;  // r2: lash hugs the eye
  s.mouthY = 53; s.mouthW = 8; s.mouthDepth = 5;
  s.blush = Blush::Dither; s.blushDX = 9; s.blushDY = 4;
  s.rest.mouthCurve = 1.2f; s.rest.mouthOpen = 0.55f;
  s.rest.blush = 1.f;
  return s;
}

// C — Minimal Premium Mascot: solid rounded slabs, no pupils; gaze moves the whole eye.
FaceStyle styleC() {
  FaceStyle s = baseStyle();
  s.id = 'C'; s.name = "Pebble"; s.motto = "minimal premium";
  s.shape = EyeShape::RoundRect; s.fill = EyeFill::Solid; s.corner = 10;
  s.eyeW = 30; s.eyeH = 38; s.spacing = 56; s.eyeY = 27;
  s.gazeShiftX = 9; s.gazeShiftY = 6; s.perspective = 0.12f;
  s.mouthW = 12;
  s.blush = Blush::None;
  s.rest.mouthW = 0.f;  // no mouth at rest — it only appears for strong emotions
  return s;
}

// D — Mischievous Cutie: lowered flat lids, side glance, one raised brow, smirk.
FaceStyle styleD() {
  FaceStyle s = baseStyle();
  s.id = 'D'; s.name = "Imp"; s.motto = "mischievous cutie";
  s.eyeW = 34; s.eyeH = 29; s.spacing = 56; s.eyeY = 30;     // r2: ellipse (r1 squircle read "camera")
  s.pupilRx = 11; s.pupilRy = 12; s.pupilDy = 1;
  s.mouthW = 12; s.fang = 2;
  s.blush = Blush::None; s.browGap = 2;
  s.rest.eye[0].open = 0.7f; s.rest.eye[1].open = 0.62f;     // asymmetric half-lids
  s.rest.eye[0].curve = -0.3f; s.rest.eye[1].curve = -0.3f;
  s.rest.eye[0].happy = 0.38f; s.rest.eye[1].happy = 0.38f;
  s.rest.gazeX = 0.65f;
  s.rest.mouthCurve = 1.4f; s.rest.mouthAsym = 1.4f;
  return s;
}

// E — Sleepy Plush: heavy lids with a drawn lid edge, big low pupils, tiny "o" mouth.
FaceStyle styleE() {
  FaceStyle s = baseStyle();
  s.id = 'E'; s.name = "Dozy"; s.motto = "sleepy plush";
  s.eyeW = 32; s.eyeH = 30; s.spacing = 58; s.eyeY = 31;
  s.pupilRx = 13; s.pupilRy = 13; s.pupilDy = 2;             // r2: big iris under a curved lid
  s.lidLine = LidLine::Lid; s.lidThick = 1; s.lidGap = 1; s.lidCurve = 0.35f;
  s.mouthW = 7;
  s.blush = Blush::Dither;
  s.rest.eye[0].open = 0.6f; s.rest.eye[1].open = 0.6f;
  s.rest.gazeY = 0.2f;
  s.rest.mouthOpen = 0.45f; s.rest.mouthRound = 1.f; s.rest.mouthCurve = 0.f;
  s.rest.blush = 1.f;
  return s;
}

// F — TinyBuddy: bean-shaped squircle eyes, star catch-light, cat mouth, hatch blush.
FaceStyle styleF() {
  FaceStyle s = baseStyle();
  s.id = 'F'; s.name = "TinyBuddy"; s.motto = "signature blend";
  s.shape = EyeShape::Squircle; s.squircleN = 2.4f;
  s.eyeW = 32; s.eyeH = 38; s.spacing = 54; s.eyeY = 28;
  s.pupilRx = 13.5f; s.pupilRy = 15; s.pupilDy = 2;          // r2: iris 13.5x15 (r1 11.5 = donut)
  s.highlight = Highlight::Sparkle;
  s.perspective = 0.05f;
  s.mouthW = 12;
  s.rest.eye[0].happy = 0.12f; s.rest.eye[1].happy = 0.12f;
  s.rest.mouthCat = 1.f; s.rest.mouthCurve = 0.6f;
  s.rest.blush = 1.f;
  return s;
}

const FaceStyle& table(int i) {
  static const FaceStyle kStyles[] = {styleA(), styleB(), styleC(), styleD(), styleE(), styleF()};
  return kStyles[i];
}

}  // namespace

int styleCount() { return 6; }
const FaceStyle& styleAt(int index) { return table(index < 0 ? 0 : (index > 5 ? 5 : index)); }
const FaceStyle* styleById(char id) {
  if (id >= 'a' && id <= 'z') id = (char)(id - 32);
  for (int i = 0; i < styleCount(); ++i)
    if (styleAt(i).id == id) return &styleAt(i);
  return nullptr;
}

}  // namespace tb
