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
  s.ringThick = 2; s.irisDither = false;
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
  s.eyeW = 30; s.eyeH = 34; s.eyeY = 27;
  s.pupilRx = 10.5f; s.pupilRy = 11; s.pupilDy = 2;
  s.rest.mouthCat = 1.f; s.rest.mouthCurve = 0.5f;
  s.rest.blush = 1.f;
  return s;
}

// B — Tiny Anime Companion: tall eyes, huge iris with dithered glow, bold lash with flick.
FaceStyle styleB() {
  FaceStyle s = baseStyle();
  s.id = 'B'; s.name = "Kira"; s.motto = "tiny anime companion";
  s.eyeW = 26; s.eyeH = 40; s.spacing = 54; s.eyeY = 30;
  s.pupilRx = 10; s.pupilRy = 14; s.pupilDy = 2.5f;
  s.highlight = Highlight::Anime; s.irisDither = true;
  s.lidLine = LidLine::Lash; s.lidThick = 2; s.lidGap = 1; s.lashFlick = 3;
  s.mouthY = 53; s.mouthW = 8; s.mouthDepth = 5;
  s.blush = Blush::Dither; s.blushDX = 7; s.blushDY = 3;
  s.rest.mouthCurve = 1.2f; s.rest.mouthOpen = 0.55f;
  s.rest.blush = 1.f;
  return s;
}

// C — Minimal Premium Mascot: solid rounded slabs, no pupils; gaze moves the whole eye.
FaceStyle styleC() {
  FaceStyle s = baseStyle();
  s.id = 'C'; s.name = "Pebble"; s.motto = "minimal premium";
  s.shape = EyeShape::RoundRect; s.fill = EyeFill::Solid; s.corner = 9;
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
  s.shape = EyeShape::Squircle; s.squircleN = 2.6f;
  s.eyeW = 32; s.eyeH = 30; s.spacing = 54; s.eyeY = 30;
  s.pupilRx = 8.5f; s.pupilRy = 8.5f; s.pupilDy = 1;
  s.mouthW = 12;
  s.blush = Blush::None;
  s.rest.eye[0].open = 0.68f; s.rest.eye[1].open = 0.58f;
  s.rest.eye[0].curve = -0.15f; s.rest.eye[1].curve = -0.15f;
  s.rest.eye[0].happy = 0.22f; s.rest.eye[1].happy = 0.22f;
  s.rest.gazeX = 0.65f;
  s.rest.browShow = 1.f;
  s.rest.eye[0].brow = 3.f; s.rest.eye[0].browTilt = -0.2f;
  s.rest.eye[1].brow = 0.f; s.rest.eye[1].browTilt = 0.25f;
  s.rest.mouthCurve = 1.6f; s.rest.mouthAsym = 1.8f;
  return s;
}

// E — Sleepy Plush: heavy lids with a drawn lid edge, big low pupils, tiny "o" mouth.
FaceStyle styleE() {
  FaceStyle s = baseStyle();
  s.id = 'E'; s.name = "Dozy"; s.motto = "sleepy plush";
  s.eyeW = 32; s.eyeH = 30; s.spacing = 58; s.eyeY = 31;
  s.pupilRx = 11.5f; s.pupilRy = 11.5f; s.pupilDy = 3;
  s.lidLine = LidLine::Lid; s.lidThick = 2; s.lidGap = 1; s.lidCurve = 0.1f;
  s.mouthW = 7;
  s.blush = Blush::Dither;
  s.rest.eye[0].open = 0.5f; s.rest.eye[1].open = 0.5f;
  s.rest.gazeY = 0.25f;
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
  s.pupilRx = 11.5f; s.pupilRy = 12.5f; s.pupilDy = 2;
  s.highlight = Highlight::Sparkle;
  s.perspective = 0.05f;
  s.mouthW = 11;
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
