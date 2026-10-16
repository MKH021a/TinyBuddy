#include "tb_sprites.h"

namespace tb {

// ---- Hearts --------------------------------------------------------------
static const Sprite kHeart[] = {
  {3, 3,
   "#.#"
   "###"
   ".#."},
  {5, 5,
   "##.##"
   "#####"
   "#####"
   ".###."
   "..#.."},
  {7, 6,
   ".##.##."
   "#######"
   "#######"
   ".#####."
   "..###.."
   "...#..."},
  {9, 8,
   ".##...##."
   "####.####"
   "#########"
   "#########"
   ".#######."
   "..#####.."
   "...###..."
   "....#...."},
  {11, 10,
   "..##...##.."
   ".####.####."
   "###########"
   "###########"
   "###########"
   ".#########."
   "..#######.."
   "...#####..."
   "....###...."
   ".....#....."},
  {13, 12,
   "..###...###.."
   ".#####.#####."
   "#############"
   "#############"
   "#############"
   "#############"
   ".###########."
   "..#########.."
   "...#######..."
   "....#####...."
   ".....###....."
   "......#......"},
};

// ---- Four-point sparkles ---------------------------------------------------
static const Sprite kSparkle[] = {
  {1, 1, "#"},
  {3, 3,
   ".#."
   "###"
   ".#."},
  {5, 5,
   "..#.."
   "..#.."
   "#####"
   "..#.."
   "..#.."},
  {7, 7,
   "...#..."
   "...#..."
   "..###.."
   "#######"
   "..###.."
   "...#..."
   "...#..."},
  {9, 9,
   "....#...."
   "....#...."
   "....#...."
   "...###..."
   "#########"
   "...###..."
   "....#...."
   "....#...."
   "....#...."},
  {11, 11,
   ".....#....."
   ".....#....."
   ".....#....."
   "....###...."
   "...#####..."
   "###########"
   "...#####..."
   "....###...."
   ".....#....."
   ".....#....."
   ".....#....."},
};

// ---- Round dots --------------------------------------------------------------
static const Sprite kDot[] = {
  {1, 1, "#"},
  {2, 2, "####"},
  {3, 3,
   ".#."
   "###"
   ".#."},
  {4, 4,
   ".##."
   "####"
   "####"
   ".##."},
  {5, 5,
   ".###."
   "#####"
   "#####"
   "#####"
   ".###."},
};

int spriteSizes(FxKind kind) {
  switch (kind) {
    case FxKind::Heart: return (int)(sizeof(kHeart) / sizeof(kHeart[0]));
    case FxKind::Sparkle: return (int)(sizeof(kSparkle) / sizeof(kSparkle[0]));
    case FxKind::Dot: return (int)(sizeof(kDot) / sizeof(kDot[0]));
    default: return 0;
  }
}

const Sprite* spriteFor(FxKind kind, int size) {
  int n = spriteSizes(kind);
  if (size <= 0 || n == 0) return nullptr;
  if (size > n) size = n;
  switch (kind) {
    case FxKind::Heart: return &kHeart[size - 1];
    case FxKind::Sparkle: return &kSparkle[size - 1];
    case FxKind::Dot: return &kDot[size - 1];
    default: return nullptr;
  }
}

static inline char spx(const Sprite& s, int x, int y) {
  if (x < 0 || y < 0 || x >= s.w || y >= s.h) return '.';
  return s.px[y * s.w + x];
}

void drawSprite(Canvas& c, const Sprite& s, int cx, int cy, bool halo, bool flipX) {
  int x0 = cx - s.w / 2;
  int y0 = cy - s.h / 2;
  if (halo) {
    for (int y = -1; y <= s.h; ++y)
      for (int x = -1; x <= s.w; ++x) {
        if (spx(s, x, y) == '#') continue;
        bool near = false;
        for (int j = -1; j <= 1 && !near; ++j)
          for (int i = -1; i <= 1 && !near; ++i)
            if (spx(s, x + i, y + j) == '#') near = true;
        if (near) c.pixel(x0 + (flipX ? s.w - 1 - x : x), y0 + y, Ink::Off);
      }
  }
  for (int y = 0; y < s.h; ++y)
    for (int x = 0; x < s.w; ++x) {
      char p = spx(s, x, y);
      if (p == '.') continue;
      c.pixel(x0 + (flipX ? s.w - 1 - x : x), y0 + y, p == '#' ? Ink::On : Ink::Off);
    }
}

void drawFx(Canvas& c, const FxList& fx) {
  for (int i = 0; i < fx.n; ++i) {
    const FxSprite& f = fx.item[i];
    const Sprite* s = spriteFor(f.kind, f.size);
    if (s) drawSprite(c, *s, f.x, f.y, f.halo);
  }
}

}  // namespace tb
