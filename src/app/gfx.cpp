#include "gfx.h"

#include <cmath>
#include <cstring>

namespace gfx {

Color blend(Color d, Color s, uint8_t a) {
  if (a == 255) return s;
  if (a == 0) return d;
  uint32_t ia = 255 - a;
  uint32_t r = (((s >> 11) & 31) * a + ((d >> 11) & 31) * ia) / 255;
  uint32_t g = (((s >> 5) & 63) * a + ((d >> 5) & 63) * ia) / 255;
  uint32_t b = ((s & 31) * a + (d & 31) * ia) / 255;
  return (Color)((r << 11) | (g << 5) | b);
}

Color mul(Color c, uint8_t r, uint8_t g, uint8_t b) {
  uint32_t R = ((c >> 11) & 31) * r / 255, G = ((c >> 5) & 63) * g / 255, B = (c & 31) * b / 255;
  return (Color)((R << 11) | (G << 5) | B);
}

static inline Color fromArgb(uint32_t v) {
  return rgb((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF);
}

void Canvas::clip(int x, int y, int w, int h) {
  cx0 = x < 0 ? 0 : x; cy0 = y < 0 ? 0 : y;
  cx1 = x + w > W ? W : x + w; cy1 = y + h > H ? H : y + h;
}

void Canvas::pixelA(int x, int y, Color c, uint8_t a) {
  if (x < cx0 || x >= cx1 || y < cy0 || y >= cy1) return;
  Color& d = px[y * W + x];
  d = blend(d, c, a);
}

void Canvas::fillRect(int x, int y, int w, int h, Color c) {
  int x0 = x < cx0 ? cx0 : x, y0 = y < cy0 ? cy0 : y;
  int x1 = x + w > cx1 ? cx1 : x + w, y1 = y + h > cy1 ? cy1 : y + h;
  for (int yy = y0; yy < y1; ++yy) {
    Color* row = px + yy * W;
    for (int xx = x0; xx < x1; ++xx) row[xx] = c;
  }
}

void Canvas::fillRectA(int x, int y, int w, int h, Color c, uint8_t a) {
  int x0 = x < cx0 ? cx0 : x, y0 = y < cy0 ? cy0 : y;
  int x1 = x + w > cx1 ? cx1 : x + w, y1 = y + h > cy1 ? cy1 : y + h;
  for (int yy = y0; yy < y1; ++yy) {
    Color* row = px + yy * W;
    for (int xx = x0; xx < x1; ++xx) row[xx] = blend(row[xx], c, a);
  }
}

void Canvas::rect(int x, int y, int w, int h, Color c, int t) {
  fillRect(x, y, w, t, c); fillRect(x, y + h - t, w, t, c);
  fillRect(x, y, t, h, c); fillRect(x + w - t, y, t, h, c);
}

void Canvas::line(int x0, int y0, int x1, int y1, Color c) {
  int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
  int dy = -(y1 > y0 ? y1 - y0 : y0 - y1), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    pixel(x0, y0, c);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

// Anti-aliased coverage of a corner pixel for rounded shapes.
static uint8_t cornerCov(float dx, float dy, float r) {
  float d = std::sqrt(dx * dx + dy * dy);
  float cov = r - d + 0.5f;
  if (cov <= 0) return 0;
  if (cov >= 1) return 255;
  return (uint8_t)(cov * 255);
}

void Canvas::fillRound(int x, int y, int w, int h, int r, Color c) {
  if (r <= 0) { fillRect(x, y, w, h, c); return; }
  fillRect(x + r, y, w - 2 * r, h, c);
  fillRect(x, y + r, r, h - 2 * r, c);
  fillRect(x + w - r, y + r, r, h - 2 * r, c);
  for (int yy = 0; yy < r; ++yy)
    for (int xx = 0; xx < r; ++xx) {
      uint8_t a = cornerCov(r - xx - 0.5f, r - yy - 0.5f, (float)r);
      if (!a) continue;
      pixelA(x + xx, y + yy, c, a);
      pixelA(x + w - 1 - xx, y + yy, c, a);
      pixelA(x + xx, y + h - 1 - yy, c, a);
      pixelA(x + w - 1 - xx, y + h - 1 - yy, c, a);
    }
}

void Canvas::roundRect(int x, int y, int w, int h, int r, Color c, int t) {
  fillRect(x + r, y, w - 2 * r, t, c);
  fillRect(x + r, y + h - t, w - 2 * r, t, c);
  fillRect(x, y + r, t, h - 2 * r, c);
  fillRect(x + w - t, y + r, t, h - 2 * r, c);
  for (int yy = 0; yy < r; ++yy)
    for (int xx = 0; xx < r; ++xx) {
      float dx = r - xx - 0.5f, dy = r - yy - 0.5f;
      float d = std::sqrt(dx * dx + dy * dy);
      float cov = 1.0f - std::fabs(d - (r - t * 0.5f)) / (t * 0.5f + 0.5f);
      if (cov <= 0) continue;
      uint8_t a = (uint8_t)(cov > 1 ? 255 : cov * 255);
      pixelA(x + xx, y + yy, c, a);
      pixelA(x + w - 1 - xx, y + yy, c, a);
      pixelA(x + xx, y + h - 1 - yy, c, a);
      pixelA(x + w - 1 - xx, y + h - 1 - yy, c, a);
    }
}

void Canvas::fillCircle(int cx, int cy, int r, Color c, uint8_t alpha) {
  fillEllipse(cx, cy, r, r, c, alpha);
}

void Canvas::circle(int cx, int cy, int r, Color c) {
  for (int yy = -r - 1; yy <= r + 1; ++yy)
    for (int xx = -r - 1; xx <= r + 1; ++xx) {
      float d = std::sqrt((float)(xx * xx + yy * yy));
      float cov = 1.0f - std::fabs(d - r);
      if (cov > 0) pixelA(cx + xx, cy + yy, c, (uint8_t)(cov * 255));
    }
}

void Canvas::fillEllipse(int cx, int cy, int rx, int ry, Color c, uint8_t alpha) {
  if (rx <= 0 || ry <= 0) return;
  for (int yy = -ry; yy <= ry; ++yy)
    for (int xx = -rx; xx <= rx; ++xx) {
      float nx = (xx) / (float)rx, ny = (yy) / (float)ry;
      float d = std::sqrt(nx * nx + ny * ny);
      float edge = (1.0f - d) * (rx < ry ? rx : ry) + 0.5f;
      if (edge <= 0) continue;
      uint8_t a = edge >= 1 ? alpha : (uint8_t)(edge * alpha);
      pixelA(cx + xx, cy + yy, c, a);
    }
}

void Canvas::vgradient(int x, int y, int w, int h, Color top, Color bottom, int steps) {
  if (steps < 1) steps = 1;
  for (int i = 0; i < steps; ++i) {
    int y0 = y + h * i / steps, y1 = y + h * (i + 1) / steps;
    uint8_t a = (uint8_t)(steps == 1 ? 0 : 255 * i / (steps - 1));
    fillRect(x, y0, w, y1 - y0, blend(top, bottom, a));
  }
}

void Canvas::sprite(const Sprite& s, int x, int y, const Tint& t, int div) {
  if (div < 1) div = 1;
  Color pal[16];
  uint8_t al[16];
  for (int i = 0; i < 16; ++i) {
    uint32_t v = s.pal[i];
    uint8_t r = (v >> 16) & 0xFF, g = (v >> 8) & 0xFF, b = v & 0xFF;
    if (t.gray) { uint8_t l = (uint8_t)((r * 77 + g * 150 + b * 29) >> 8); r = g = b = l; }
    pal[i] = mul(rgb(r, g, b), t.r, t.g, t.b);
    al[i] = (uint8_t)(((v >> 24) & 0xFF) * t.a / 255);
  }
  int rowBytes = (s.w + 1) / 2;
  int ow = s.w / div, oh = s.h / div;
  x += s.ox / div;
  y += s.oy / div;
  for (int yy = 0; yy < oh; ++yy) {
    int py = y + yy;
    if (py < cy0 || py >= cy1) continue;
    const uint8_t* row = s.idx + (yy * div) * rowBytes;
    for (int xx = 0; xx < ow; ++xx) {
      int px_ = x + xx;
      if (px_ < cx0 || px_ >= cx1) continue;
      int sx = xx * div;
      uint8_t byte = row[sx >> 1];
      uint8_t i = (sx & 1) ? (byte & 15) : (byte >> 4);
      uint8_t a = al[i];
      if (!a) continue;
      Color& d = px[py * W + px_];
      d = a == 255 ? pal[i] : blend(d, pal[i], a);
    }
  }
}

void Canvas::moon(int cx, int cy, int r, float phase, Color lit, Color dark) {
  // Terminator: an ellipse with x radius r*cos(2*pi*phase).
  float k = std::cos(2.0f * 3.14159265f * phase);   // 1 new, -1 full
  bool waxing = phase < 0.5f;
  for (int yy = -r; yy <= r; ++yy) {
    float half = std::sqrt((float)(r * r - yy * yy));
    for (int xx = -r; xx <= r; ++xx) {
      float d = std::sqrt((float)(xx * xx + yy * yy));
      float cov = r - d + 0.5f;
      if (cov <= 0) continue;
      uint8_t a = cov >= 1 ? 255 : (uint8_t)(cov * 255);
      float tx = half * k;                 // terminator x at this row
      bool isLit = waxing ? (xx > tx) : (xx < -tx);
      pixelA(cx + xx, cy + yy, isLit ? lit : dark, a);
    }
  }
}

// ---- text ------------------------------------------------------------------
static const Glyph* glyphOf(const Font& f, char ch) {
  unsigned char c = (unsigned char)ch;
  if (c < f.first || c > f.last) c = '?';
  return &f.glyphs[c - f.first];
}

int Canvas::text(const Font& f, int x, int y, const char* s, Color c, uint8_t alpha) {
  int x0 = x;
  for (; *s; ++s) {
    if (*s == '\n') break;
    const Glyph* g = glyphOf(f, *s);
    int rb = (g->w + 1) / 2;
    const uint8_t* bits = f.bits + g->offset;
    for (int yy = 0; yy < g->h; ++yy)
      for (int xx = 0; xx < g->w; ++xx) {
        uint8_t byte = bits[yy * rb + (xx >> 1)];
        uint8_t v = (xx & 1) ? (byte & 15) : (byte >> 4);
        if (!v) continue;
        uint8_t a = (uint8_t)(v * 17 * alpha / 255);
        pixelA(x + g->xoff + xx, y + g->yoff + yy, c, a);
      }
    x += g->adv;
  }
  return x - x0;
}

int Canvas::textWidth(const Font& f, const char* s) {
  int w = 0;
  for (; *s && *s != '\n'; ++s) w += glyphOf(f, *s)->adv;
  return w;
}

// Measure the next line that fits in w; returns its length in chars.
static int fitLine(const Font& f, const char* s, int w, int* outWidth) {
  int width = 0, lastBreak = -1, lastBreakW = 0, i = 0;
  for (; s[i] && s[i] != '\n'; ++i) {
    if (s[i] == ' ') { lastBreak = i; lastBreakW = width; }
    int adv = glyphOf(f, s[i])->adv;
    if (width + adv > w && i > 0) {
      if (lastBreak > 0) { *outWidth = lastBreakW; return lastBreak; }
      *outWidth = width; return i;
    }
    width += adv;
  }
  *outWidth = width;
  return i;
}

int Canvas::textWrap(const Font& f, int x, int y, int w, const char* s, Color c, int lineGap,
                     int maxLines, bool center) {
  char buf[96];
  int lines = 0;
  while (*s) {
    int lw = 0;
    int n = fitLine(f, s, w, &lw);
    int m = n < (int)sizeof(buf) - 1 ? n : (int)sizeof(buf) - 1;
    std::memcpy(buf, s, m);
    buf[m] = 0;
    if (maxLines && lines == maxLines - 1 && s[n]) {   // ellipsize the last allowed line
      int k = m;
      while (k > 0 && textWidth(f, buf) + textWidth(f, "...") > w) buf[--k] = 0;
      std::strcat(buf, "...");
    }
    text(f, center ? x + (w - textWidth(f, buf)) / 2 : x, y, buf, c);
    y += f.line + lineGap;
    ++lines;
    s += n;
    while (*s == ' ') ++s;
    if (*s == '\n') ++s;
    if (maxLines && lines >= maxLines) break;
  }
  return y;
}

int Canvas::wrapLines(const Font& f, int w, const char* s) {
  int lines = 0;
  while (*s) {
    int lw = 0;
    int n = fitLine(f, s, w, &lw);
    ++lines;
    s += n;
    while (*s == ' ') ++s;
    if (*s == '\n') ++s;
  }
  return lines;
}

}  // namespace gfx
