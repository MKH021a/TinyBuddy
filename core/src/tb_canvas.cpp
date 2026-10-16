#include "tb_canvas.h"
#include <string.h>

namespace tb {

void Canvas::clear() { memset(buf_, 0, kBufBytes); }

void Canvas::invert() {
  for (int i = 0; i < kBufBytes; ++i) buf_[i] = (uint8_t)~buf_[i];
}

static inline void applyInk(uint8_t& b, uint8_t mask, Ink ink) {
  switch (ink) {
    case Ink::On: b |= mask; break;
    case Ink::Off: b &= (uint8_t)~mask; break;
    case Ink::Flip: b ^= mask; break;
  }
}

void Canvas::pixel(int x, int y, Ink ink) {
  if ((unsigned)x >= (unsigned)kW || (unsigned)y >= (unsigned)kH) return;
  applyInk(buf_[(y >> 3) * kW + x], (uint8_t)(1u << (y & 7)), ink);
}

bool Canvas::get(int x, int y) const {
  if ((unsigned)x >= (unsigned)kW || (unsigned)y >= (unsigned)kH) return false;
  return (buf_[(y >> 3) * kW + x] >> (y & 7)) & 1u;
}

void Canvas::vspan(int x, int y0, int y1, Ink ink) {
  if ((unsigned)x >= (unsigned)kW) return;
  if (y0 < 0) y0 = 0;
  if (y1 > kH - 1) y1 = kH - 1;
  if (y0 > y1) return;
  // Vertical spans are the cheap direction in page layout: one masked byte per 8 rows.
  for (int page = y0 >> 3; page <= (y1 >> 3); ++page) {
    int top = page * 8;
    int s = imax(top, y0) - top;
    int e = imin(top + 7, y1) - top;
    uint8_t mask = (uint8_t)((0xFFu << s) & (0xFFu >> (7 - e)));
    applyInk(buf_[page * kW + x], mask, ink);
  }
}

void Canvas::hspan(int x0, int x1, int y, Ink ink) {
  if ((unsigned)y >= (unsigned)kH) return;
  if (x0 < 0) x0 = 0;
  if (x1 > kW - 1) x1 = kW - 1;
  uint8_t mask = (uint8_t)(1u << (y & 7));
  uint8_t* row = buf_ + (y >> 3) * kW;
  for (int x = x0; x <= x1; ++x) applyInk(row[x], mask, ink);
}

void Canvas::rect(int x, int y, int w, int h, Ink ink) {
  for (int i = 0; i < w; ++i) vspan(x + i, y, y + h - 1, ink);
}

void Canvas::frame(int x, int y, int w, int h, Ink ink) {
  hspan(x, x + w - 1, y, ink);
  hspan(x, x + w - 1, y + h - 1, ink);
  vspan(x, y, y + h - 1, ink);
  vspan(x + w - 1, y, y + h - 1, ink);
}

int Canvas::countLit() const {
  int n = 0;
  for (int i = 0; i < kBufBytes; ++i) {
    uint8_t b = buf_[i];
    while (b) { b &= (uint8_t)(b - 1); ++n; }
  }
  return n;
}

}  // namespace tb
