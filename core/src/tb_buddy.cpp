#include "tb_buddy.h"
#include "tb_catalog.h"
#include "tb_ease.h"

namespace tb {

namespace {
constexpr uint32_t kRecoverMs = 280;
}

void Buddy::begin(const FaceStyle& st, uint32_t seed, uint32_t now) {
  st_ = &st;
  rng_.reseed(seed ^ 0xB0DD1E5u);
  idle_.reset(seed, now);
  anim_ = nullptr;
  recovering_ = false;
  fx_.clear();
  idle_.pose(st, now, face_);
  scheduleAuto(now, 6000);
}

void Buddy::scheduleAuto(uint32_t now, uint32_t afterMs) {
  nextAuto_ = now + afterMs + (uint32_t)rng_.irange(0, 7000);
}

const AnimDef* Buddy::pickAuto() {
  // Weighted pick, avoiding an immediate repeat when there is a choice.
  // (Cooldowns, categories and personality arrive with the full catalog.)
  int total = 0;
  for (int i = 0; i < animCount(); ++i)
    if (animCount() == 1 || animAt(i).id != lastAutoId_) total += animAt(i).weight;
  if (total <= 0) return nullptr;
  int r = rng_.irange(0, total - 1);
  for (int i = 0; i < animCount(); ++i) {
    const AnimDef& a = animAt(i);
    if (animCount() != 1 && a.id == lastAutoId_) continue;
    if ((r -= a.weight) < 0) return &a;
  }
  return &animAt(0);
}

bool Buddy::play(const AnimDef& a, uint32_t now, int mirror) {
  if (anim_ && !anim_->interruptible) return false;
  // base = the blink-free pose right now (or the face mid-recovery), so the first keys
  // tween from exactly what is on screen
  FaceState pose;
  idle_.pose(*st_, now, pose, false);
  base_ = pose;
  var_.mirror = mirror < 0 ? rng_.chance(0.5f) : mirror == 1;
  var_.timeScale = rng_.range(0.96f, 1.05f);  // controlled tempo variation
  var_.seed = rng_.next();
  anim_ = &a;
  animStart_ = now;
  animDur_ = scaledDuration(a, var_);
  recovering_ = false;
  ++plays_;
  return true;
}

void Buddy::stop(uint32_t now) {
  if (!anim_) return;
  recoverFrom_ = face_;
  recovering_ = true;
  recoverStart_ = now;
  anim_ = nullptr;
  fx_.clear();  // temporary effects never outlive their animation
  idle_.holdOff(now, 500);
}

void Buddy::setAuto(bool on, uint32_t now) {
  auto_ = on;
  if (on) scheduleAuto(now, 1500);
}

void Buddy::update(uint32_t now) {
  if (anim_) {
    uint32_t t = now - animStart_;
    if (t >= animDur_) {
      lastAutoId_ = anim_->id;
      anim_ = nullptr;
      fx_.clear();
      idle_.holdOff(now, 700);  // no blink right after the closing blink
      scheduleAuto(now, 9000);
    } else {
      evaluate(*anim_, t, var_, *st_, base_, face_, fx_);
      if (anim_->idleBlinks) {
        float b = idle_.blinkFactor(now);
        for (EyeState& e : face_.eye) e.open *= b;
      }
      return;
    }
  }
  fx_.clear();
  FaceState idle;
  idle_.pose(*st_, now, idle);
  if (recovering_) {
    float p = (now - recoverStart_) / (float)kRecoverMs;
    if (p >= 1.f) recovering_ = false;
    else { blendFaces(recoverFrom_, idle, ease(Ease::InOutQuad, p), face_); return; }
  }
  face_ = idle;
  if (auto_ && (int32_t)(now - nextAuto_) >= 0) {
    const AnimDef* a = pickAuto();
    if (a) play(*a, now);
    else scheduleAuto(now, 9000);
  }
}

void Buddy::render(Canvas& c, RenderStats* stats) const {
  renderFace(c, *st_, face_, &fx_, stats);
}

}  // namespace tb
