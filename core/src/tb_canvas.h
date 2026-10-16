#pragma once
#include "tb_types.h"

namespace tb {

enum class Ink : uint8_t { Off = 0, On = 1, Flip = 2 };

// 128x64 1-bit canvas in "page" layout: byte = buf[(y >> 3) * 128 + x], bit = y & 7 (LSB on top).
// This is the native memory layout of SSD1306, SH1106 and U8g2 full-buffer mode, so the
// firmware can render straight into the display library's buffer with zero copies.
class Canvas {
 public:
  explicit Canvas(uint8_t* buf) : buf_(buf) {}
  uint8_t* data() { return buf_; }
  const uint8_t* data() const { return buf_; }

  void clear();
  void invert();
  void pixel(int x, int y, Ink ink);
  bool get(int x, int y) const;
  void vspan(int x, int y0, int y1, Ink ink);  // inclusive, clipped
  void hspan(int x0, int x1, int y, Ink ink);  // inclusive, clipped
  void rect(int x, int y, int w, int h, Ink ink);
  void frame(int x, int y, int w, int h, Ink ink);
  int countLit() const;

 private:
  uint8_t* buf_;
};

}  // namespace tb
