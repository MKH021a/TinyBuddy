#pragma once
#include "tb_canvas.h"
#include "tb_face.h"

namespace tb {

// Hand-placed pixel art. At 3-13 px, hand-drawn pixels beat any analytic shape.
// Rows are ASCII so the art stays readable/editable in source:
//   '#' lit   'o' forced dark   '.' transparent
struct Sprite {
  uint8_t w, h;
  const char* px;  // w*h chars, row-major
};

// Number of size steps available for an effect kind (size index 1..count).
int spriteSizes(FxKind kind);
const Sprite* spriteFor(FxKind kind, int size);

// Draws centred at (cx, cy). Halo clears a 1px ring around lit pixels first.
void drawSprite(Canvas& c, const Sprite& s, int cx, int cy, bool halo, bool flipX = false);
void drawFx(Canvas& c, const FxList& fx);

}  // namespace tb
