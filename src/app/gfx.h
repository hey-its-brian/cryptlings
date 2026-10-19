// Tiny software renderer for a 172 x 320 RGB565 framebuffer.
// Portable (no Arduino): the board pushes the buffer over SPI, the host
// simulator writes it to an image. Pixels are native-endian RGB565.
#pragma once
#include <cstdint>
#include <cstddef>

namespace gfx {

static const int W = 172;
static const int H = 320;

typedef uint16_t Color;

constexpr Color rgb(uint8_t r, uint8_t g, uint8_t b) {
  return (Color)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}
constexpr Color hex(uint32_t v) { return rgb((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF); }
Color blend(Color dst, Color src, uint8_t a);         // a: 0 = dst, 255 = src
Color mul(Color c, uint8_t r, uint8_t g, uint8_t b);  // per-channel multiply (tint), 255 = keep

}  // namespace gfx

// A 4bpp alpha glyph (fonts) -------------------------------------------------
struct Glyph {
  uint32_t offset;   // into Font::bits
  uint8_t w, h;
  int8_t xoff, yoff; // from pen position on the baseline
  uint8_t adv;
};
struct Font {
  const uint8_t* bits;
  const Glyph* glyphs;
  uint8_t first, last;
  int8_t ascent;
  int8_t line;
};

// A 4bpp indexed sprite with an RGBA palette (index alpha 0 = transparent) ---
struct Sprite {
  uint8_t w, h;          // cropped size
  uint8_t ox, oy;        // offset of the crop inside the original art box
  const uint32_t* pal;   // 16 x 0xAARRGGBB
  const uint8_t* idx;    // w*h/2 bytes, high nibble first, rows padded to bytes
};

namespace gfx {

struct Tint { uint8_t r = 255, g = 255, b = 255, a = 255; bool gray = false; };

class Canvas {
 public:
  Color* px;           // W*H
  explicit Canvas(Color* buf) : px(buf) { noClip(); }

  void clip(int x, int y, int w, int h);
  void noClip() { cx0 = 0; cy0 = 0; cx1 = W; cy1 = H; }

  void fill(Color c) { fillRect(0, 0, W, H, c); }
  void pixel(int x, int y, Color c) { if (x >= cx0 && x < cx1 && y >= cy0 && y < cy1) px[y * W + x] = c; }
  void pixelA(int x, int y, Color c, uint8_t a);
  void fillRect(int x, int y, int w, int h, Color c);
  void fillRectA(int x, int y, int w, int h, Color c, uint8_t a);
  void rect(int x, int y, int w, int h, Color c, int t = 1);
  void hline(int x, int y, int w, Color c) { fillRect(x, y, w, 1, c); }
  void vline(int x, int y, int h, Color c) { fillRect(x, y, 1, h, c); }
  void line(int x0, int y0, int x1, int y1, Color c);
  void fillRound(int x, int y, int w, int h, int r, Color c);
  void roundRect(int x, int y, int w, int h, int r, Color c, int t = 1);
  void fillCircle(int cx, int cy, int r, Color c, uint8_t a = 255);
  void circle(int cx, int cy, int r, Color c);
  void fillEllipse(int cx, int cy, int rx, int ry, Color c, uint8_t a = 255);
  void vgradient(int x, int y, int w, int h, Color top, Color bottom, int steps = 6);

  // Sprites: scale is 1 (normal), or a divisor (2 = half size, nearest).
  void sprite(const Sprite& s, int x, int y, const Tint& t = Tint(), int div = 1);
  // Moon disc: phase 0 new, 0.5 full (northern hemisphere: waxing lit right).
  void moon(int cx, int cy, int r, float phase, Color lit, Color dark);

  // Text: y is the baseline. Returns the advance width.
  int text(const Font& f, int x, int y, const char* s, Color c, uint8_t a = 255);
  static int textWidth(const Font& f, const char* s);
  void textCenter(const Font& f, int cx, int y, const char* s, Color c, uint8_t a = 255) {
    text(f, cx - textWidth(f, s) / 2, y, s, c, a);
  }
  // Word-wrapped paragraph. Returns the y of the next free line. maxLines 0 = no limit.
  int textWrap(const Font& f, int x, int y, int w, const char* s, Color c, int lineGap = 2,
               int maxLines = 0, bool center = false);
  static int wrapLines(const Font& f, int w, const char* s);

 private:
  int cx0, cy0, cx1, cy1;
};

}  // namespace gfx
