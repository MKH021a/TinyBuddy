#pragma once
#include "tb_types.h"

namespace tb {

// Small deterministic PRNG (xorshift32). Same seed -> same behaviour on desktop and device,
// which is what makes `test` runs and preview renders reproducible.
class Rng {
 public:
  explicit Rng(uint32_t seed = 0x7B1D5EEDu) { reseed(seed); }
  void reseed(uint32_t seed) { s_ = seed ? seed : 0x9E3779B9u; }
  uint32_t next() {
    uint32_t x = s_;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return s_ = x;
  }
  float unit() { return (next() >> 8) * (1.0f / 16777216.0f); }       // [0,1)
  float range(float lo, float hi) { return lo + (hi - lo) * unit(); }
  int irange(int lo, int hi) { return lo + (int)(next() % (uint32_t)(hi - lo + 1)); }  // inclusive
  bool chance(float p) { return unit() < p; }
  uint32_t state() const { return s_; }

 private:
  uint32_t s_;
};

// Stateless hash -> [0,1). Used for per-effect jitter so evaluation stays a pure function of time.
inline float hash01(uint32_t a, uint32_t b) {
  uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u;
  h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
  return (h >> 8) * (1.0f / 16777216.0f);
}

}  // namespace tb
