#pragma once
#include "tb_canvas.h"
#include "tb_face.h"

namespace tb {

// Debug/QA counters filled by renderFace (cheap; used by tests and the `fps` command).
struct RenderStats {
  int eyeColumns = 0;
  int litPixels = 0;
};

// Renders one complete frame (clears the canvas first).
void renderFace(Canvas& c, const FaceStyle& st, const FaceState& f, const FxList* fx,
                RenderStats* stats = nullptr);

// Geometry helpers shared with the animation layer (effect placement etc).
struct EyeBox { float cx, cy, a, b; };
EyeBox eyeBox(const FaceStyle& st, const FaceState& f, int eye);

}  // namespace tb
