// TinyBuddy core — shared by the desktop preview (native) and the ESP32-C3 firmware.
// Pure C++, no Arduino, no heap. Geometry uses float per eye/column; per-pixel work is integer.
#pragma once
#include <stdint.h>
#include <math.h>

namespace tb {

constexpr int kW = 128;
constexpr int kH = 64;
constexpr int kBufBytes = kW * kH / 8;  // 1024 bytes, SSD1306/SH1106/U8g2 page layout
constexpr float kPi = 3.14159265f;

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline int imin(int a, int b) { return a < b ? a : b; }
inline int imax(int a, int b) { return a > b ? a : b; }
inline float fminf2(float a, float b) { return a < b ? a : b; }
inline float fmaxf2(float a, float b) { return a > b ? a : b; }
inline float sq(float v) { return v * v; }

// Pixel (x) has its centre at x + 0.5. These convert a continuous interval [lo, hi]
// into the inclusive range of pixel indices whose centres fall inside it.
inline int firstPx(float lo) { return (int)ceilf(lo - 0.5f); }
inline int lastPx(float hi) { return (int)floorf(hi - 0.5f); }

}  // namespace tb
