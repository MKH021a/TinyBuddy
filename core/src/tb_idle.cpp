#include "tb_idle.h"
#include "tb_ease.h"

namespace tb {

namespace {
constexpr uint32_t kMicroLen = 1700;
}

void Idle::reset(uint32_t seed, uint32_t now) {
  rng_.reseed(seed);
  t0_ = now;
  gx_ = gy_ = fromX_ = fromY_ = toX_ = toY_ = 0;
  sacStart_ = now; sacDur_ = 1;
  nextSac_ = now + (uint32_t)rng_.irange(900, 2200);
  away_ = false;
  blinkKind_ = kNone;
  scheduleBlink(now, 1200);
  microOn_ = false;
  nextMicro_ = now + (uint32_t)rng_.irange(9000, 16000);
}

void Idle::holdOff(uint32_t now, uint32_t ms) {
  if (!due(blinkAt_, now + ms) || blinkKind_ != kNone) { blinkKind_ = kNone; blinkAt_ = now + ms + (uint32_t)rng_.irange(400, 2200); }
  if (!due(nextSac_, now + ms)) nextSac_ = now + ms + (uint32_t)rng_.irange(300, 1200);
  if (!due(nextMicro_, now + ms)) nextMicro_ = now + ms + (uint32_t)rng_.irange(4000, 9000);
  microOn_ = false;
  // freeze any in-flight saccade at its current position
  fromX_ = toX_ = gx_;
  fromY_ = toY_ = gy_;
  sacStart_ = now; sacDur_ = 1;
}

void Idle::scheduleBlink(uint32_t now, uint32_t minGap) {
  // humans blink every ~2-6 s; a cartoon reads better slightly calmer
  blinkAt_ = now + minGap + (uint32_t)rng_.irange(1400, 4600);
}

void Idle::startSaccade(uint32_t now) {
  fromX_ = gx_;
  fromY_ = gy_;
  float r = rng_.unit();
  uint32_t hold;
  if (away_ || r < 0.55f) {          // drift near the viewer (eye contact)
    toX_ = rng_.range(-0.14f, 0.14f);
    toY_ = rng_.range(-0.08f, 0.10f);
    hold = (uint32_t)rng_.irange(1700, 4200);
    away_ = false;
  } else if (r < 0.86f) {            // casual glance
    toX_ = rng_.range(-0.55f, 0.55f);
    toY_ = rng_.range(-0.30f, 0.25f);
    hold = (uint32_t)rng_.irange(900, 2200);
  } else {                           // brief look-away, then come back
    toX_ = rng_.chance(0.5f) ? rng_.range(-0.95f, -0.7f) : rng_.range(0.7f, 0.95f);
    toY_ = rng_.range(-0.4f, 0.3f);
    hold = (uint32_t)rng_.irange(600, 1300);
    away_ = true;
  }
  float dist = fabsf(toX_ - fromX_) + fabsf(toY_ - fromY_);
  sacStart_ = now;
  sacDur_ = 70 + (uint32_t)(110.f * fminf2(dist, 1.6f));
  nextSac_ = now + sacDur_ + hold;
  // big gaze shifts are often accompanied by a blink
  if (dist > 0.6f && blinkKind_ == kNone && rng_.chance(0.45f)) {
    blinkKind_ = kNormal;
    blinkStart_ = now;
  }
}

float Idle::blinkCurve(uint32_t el) const {
  // returns lid openness factor for a blink that started `el` ms ago (1 = open)
  auto single = [](uint32_t e, uint32_t close, uint32_t hold, uint32_t open, float depth) -> float {
    if (e < close) return 1.f - depth * ease(Ease::InQuad, e / (float)close);
    e -= close;
    if (e < hold) return 1.f - depth;
    e -= hold;
    if (e < open) return 1.f - depth * (1.f - ease(Ease::OutCubic, e / (float)open));
    return 1.f;
  };
  switch (blinkKind_) {
    case kNormal: return single(el, 70, 40, 130, 1.f);
    case kDouble:
      if (el < 240) return single(el, 70, 40, 130, 1.f);
      if (el < 360) return 1.f;
      return single(el - 360, 55, 25, 110, 1.f);
    case kSlow: return single(el, 170, 130, 320, 1.f);
    case kFlutter:
      if (el < 190) return single(el, 60, 20, 110, 0.55f);
      return single(el - 190, 60, 20, 110, 0.55f);
    default: return 1.f;
  }
}

static uint32_t blinkLength(uint8_t kind) {
  switch (kind) {
    case 1: return 240;
    case 2: return 525;
    case 3: return 620;
    case 4: return 380;
    default: return 0;
  }
}

void Idle::update(uint32_t now) {
  // blink schedule
  if (blinkKind_ == kNone && due(now, blinkAt_)) {
    float r = rng_.unit();
    blinkKind_ = r < 0.68f ? kNormal : r < 0.85f ? kDouble : r < 0.95f ? kSlow : kFlutter;
    blinkStart_ = now;
  }
  if (blinkKind_ != kNone && (now - blinkStart_) >= blinkLength(blinkKind_)) {
    blinkKind_ = kNone;
    scheduleBlink(now, 300);
  }
  // gaze
  if (due(now, nextSac_)) startSaccade(now);
  float p = clampf((now - sacStart_) / (float)sacDur_, 0.f, 1.f);
  float e = ease(Ease::OutQuart, p);
  gx_ = lerpf(fromX_, toX_, e);
  gy_ = lerpf(fromY_, toY_, e);
  // micro-expressions
  if (!microOn_ && due(now, nextMicro_)) {
    microOn_ = true;
    microStart_ = now;
    microKind_ = (uint8_t)rng_.irange(0, 2);
  }
  if (microOn_ && (now - microStart_) >= kMicroLen) {
    microOn_ = false;
    nextMicro_ = now + (uint32_t)rng_.irange(10000, 22000);
  }
}

float Idle::blinkFactor(uint32_t now) {
  update(now);
  return blinkKind_ == kNone ? 1.f : blinkCurve(now - blinkStart_);
}

void Idle::pose(const FaceStyle& st, uint32_t now, FaceState& out, bool withBlink) {
  update(now);
  out = st.rest;
  out.gazeX += gx_;
  out.gazeY += gy_;
  // breathing: a crisp 1px bob (sub-pixel motion on 1-bit pixels would shimmer instead)
  float br = sinf(2.f * kPi * (float)((now - t0_) % 4300u) / 4300.f);
  out.faceY += br > 0.82f ? -1.f : (br < -0.82f ? 1.f : 0.f);
  if (microOn_) {
    float m = (now - microStart_) / (float)kMicroLen;
    float env = ease(Ease::InOutSine, clampf(m / 0.2f, 0.f, 1.f)) * ease(Ease::InOutSine, clampf((1.f - m) / 0.3f, 0.f, 1.f));
    switch (microKind_) {
      case 0:  // tiny content smile
        out.mouthCurve += 0.9f * env;
        for (EyeState& e : out.eye) e.happy += 0.18f * env;
        break;
      case 1:  // perk up: eyes a touch bigger, pupils dilate
        for (EyeState& e : out.eye) { e.scaleH += 0.05f * env; e.pupil += 0.07f * env; }
        break;
      default:  // lids relax
        for (EyeState& e : out.eye) e.open *= 1.f - 0.15f * env;
        break;
    }
  }
  if (withBlink && blinkKind_ != kNone) {
    float b = blinkCurve(now - blinkStart_);
    for (EyeState& e : out.eye) e.open *= b;
  }
}

}  // namespace tb
