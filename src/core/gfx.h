// Small anti-aliased software renderer for a 240x240 RGB565 frame buffer.
// Plain C++ (no Arduino), so the same code runs on the ESP32 and on a PC preview.
#pragma once
#include <math.h>
#include <stdint.h>
#include <string.h>

static const float kPI = 3.14159265f;

struct Col { float r, g, b; };
static inline Col C(float r, float g, float b) { return {r, g, b}; }
static inline Col mixc(Col a, Col b, float t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t}; }
static inline Col mulc(Col a, float k) { return {a.r * k, a.g * k, a.b * k}; }
static inline Col addc(Col a, Col b) { return {a.r + b.r, a.g + b.g, a.b + b.b}; }

static inline float sat(float x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }
static inline float clampf(float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static inline float smoothstepf(float e0, float e1, float x) { float t = sat((x - e0) / (e1 - e0)); return t * t * (3 - 2 * t); }
static inline float easeInOut(float t) { t = sat(t); return t * t * (3 - 2 * t); }
static inline float easeOutCubic(float t) { t = 1 - sat(t); return 1 - t * t * t; }

// HSV (h in degrees, s/v 0..1) -> 0..255 colour
static inline Col hsv(float h, float s, float v) {
  h = fmodf(h, 360.f); if (h < 0) h += 360.f;
  float c = v * s, x = c * (1 - fabsf(fmodf(h / 60.f, 2.f) - 1)), m = v - c;
  float r, g, b;
  if (h < 60) { r = c; g = x; b = 0; } else if (h < 120) { r = x; g = c; b = 0; }
  else if (h < 180) { r = 0; g = c; b = x; } else if (h < 240) { r = 0; g = x; b = c; }
  else if (h < 300) { r = x; g = 0; b = c; } else { r = c; g = 0; b = x; }
  return {(r + m) * 255, (g + m) * 255, (b + m) * 255};
}

static inline float fastAtan2(float y, float x) {
  float ax = fabsf(x), ay = fabsf(y);
  float a = (ax < ay ? ax : ay) / ((ax > ay ? ax : ay) + 1e-9f);
  float s = a * a;
  float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
  if (ay > ax) r = 1.57079637f - r;
  if (x < 0) r = 3.14159274f - r;
  if (y < 0) r = -r;
  return r;
}

// tiny deterministic RNG (xorshift)
struct Rng {
  uint32_t s = 0x9E3779B9u;
  uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
  float f(float a, float b) { return a + (b - a) * ((next() & 0xFFFFFF) / 16777215.0f); }
  int i(int n) { return (int)(next() % (uint32_t)n); }
};
extern Rng rng;

struct Canvas {
  uint16_t *buf = nullptr;
  int w = 240, h = 240;
  int y0 = 0, y1 = 240;  // rows this canvas may touch (lets two CPU cores split the frame)

  // RGB565 stored byte-swapped (what the LCD / LovyanGFX sprite expects).
  // A 4x4 ordered dither hides colour banding in the soft gradients.
  inline uint16_t pack(int x, int y, Col c) const {
    static const int8_t bayer[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
    float d = bayer[((y & 3) << 2) | (x & 3)] * (1.0f / 16.0f) - 0.47f;
    int R = (int)(c.r + d * 8), G = (int)(c.g + d * 4), B = (int)(c.b + d * 8);
    R = R < 0 ? 0 : (R > 255 ? 255 : R);
    G = G < 0 ? 0 : (G > 255 ? 255 : G);
    B = B < 0 ? 0 : (B > 255 ? 255 : B);
    uint16_t v = (uint16_t)(((R & 0xF8) << 8) | ((G & 0xFC) << 3) | (B >> 3));
    return (uint16_t)((v >> 8) | (v << 8));
  }
  inline Col get(int x, int y) const {
    uint16_t s = buf[y * w + x];
    uint16_t v = (uint16_t)((s >> 8) | (s << 8));
    return {((v >> 11) & 31) * (255.f / 31.f), ((v >> 5) & 63) * (255.f / 63.f), (v & 31) * (255.f / 31.f)};
  }
  inline void put(int x, int y, Col c) { buf[y * w + x] = pack(x, y, c); }
  inline void blend(int x, int y, Col c, float a) {
    if (a <= 0.004f) return;
    if (a >= 0.996f) { put(x, y, c); return; }
    put(x, y, mixc(get(x, y), c, a));
  }
  void clearBlack() { memset(buf + y0 * w, 0, (size_t)(y1 - y0) * w * 2); }
  void fill(Col c) {
    for (int y = y0; y < y1; y++)
      for (int x = 0; x < w; x++) put(x, y, c);
  }
  // Like shade(), but evaluates f once per 2x2 block (4x cheaper). For big soft shapes.
  template <class F>
  void shade2(float bx0, float by0, float bx1, float by1, F f) {
    int xa = ((int)floorf(bx0)) & ~1, xb = (int)ceilf(bx1), ya = ((int)floorf(by0)) & ~1, yb = (int)ceilf(by1);
    if (xa < 0) xa = 0; if (xb > w) xb = w; if (ya < y0) ya = y0; if (yb > y1) yb = y1;
    for (int y = ya; y < yb; y += 2)
      for (int x = xa; x < xb; x += 2) {
        Col c; float a;
        if (!f(x + 1.0f, y + 1.0f, c, a)) continue;
        blend(x, y, c, a);
        if (x + 1 < xb) blend(x + 1, y, c, a);
        if (y + 1 < yb) { blend(x, y + 1, c, a); if (x + 1 < xb) blend(x + 1, y + 1, c, a); }
      }
  }
  // Adaptive: f(px, py, colour&, alpha&, edge&) also reports the distance to the nearest edge.
  // Flat areas (edge > 3 px) are shaded once per 2x2 block; near edges every pixel is shaded,
  // so outlines stay smooth and anti-aliased.
  template <class F>
  void shadeA(float bx0, float by0, float bx1, float by1, F f) {
    int xa = ((int)floorf(bx0)) & ~1, xb = (int)ceilf(bx1), ya = ((int)floorf(by0)) & ~1, yb = (int)ceilf(by1);
    if (xa < 0) xa = 0; if (xb > w) xb = w; if (ya < y0) ya = y0; if (yb > y1) yb = y1;
    for (int y = ya; y < yb; y += 2)
      for (int x = xa; x < xb; x += 2) {
        Col c; float a, e = 0;
        bool hit = f(x + 1.0f, y + 1.0f, c, a, e);
        if (e > 2.0f) {
          if (!hit) continue;
          for (int j = 0; j < 2 && y + j < yb; j++)
            for (int i = 0; i < 2 && x + i < xb; i++) blend(x + i, y + j, c, a);
        } else {
          for (int j = 0; j < 2 && y + j < yb; j++)
            for (int i = 0; i < 2 && x + i < xb; i++)
              if (f(x + i + 0.5f, y + j + 0.5f, c, a, e)) blend(x + i, y + j, c, a);
        }
      }
  }
  // Run f(px, py, colour&, alpha&) for every pixel in a box (clipped). f returns false to skip.
  template <class F>
  void shade(float bx0, float by0, float bx1, float by1, F f) {
    int xa = (int)floorf(bx0), xb = (int)ceilf(bx1), ya = (int)floorf(by0), yb = (int)ceilf(by1);
    if (xa < 0) xa = 0; if (xb > w) xb = w; if (ya < y0) ya = y0; if (yb > y1) yb = y1;
    for (int y = ya; y < yb; y++)
      for (int x = xa; x < xb; x++) {
        Col c; float a;
        if (f(x + 0.5f, y + 0.5f, c, a)) blend(x, y, c, a);
      }
  }
};

// ---------- signed distance helpers (negative = inside) ----------
static inline float len2(float x, float y) { return sqrtf(x * x + y * y); }

// cheap ellipse distance in pixels (good near the edge, which is all AA needs)
static inline float sdEllipse(float x, float y, float rx, float ry) {
  float k0 = len2(x / rx, y / ry), k1 = len2(x / (rx * rx), y / (ry * ry));
  return k0 * (k0 - 1.0f) / (k1 + 1e-6f);
}
// 5-point star pointing up (screen coords, y down). r = outer radius, rf = inner ratio
static inline float sdStar5(float x, float y, float r, float rf) {
  y = -y;
  const float k1x = 0.809016994f, k1y = -0.587785252f, k2x = -k1x, k2y = k1y;
  x = fabsf(x);
  float d = k1x * x + k1y * y; if (d > 0) { x -= 2 * d * k1x; y -= 2 * d * k1y; }
  d = k2x * x + k2y * y; if (d > 0) { x -= 2 * d * k2x; y -= 2 * d * k2y; }
  x = fabsf(x); y -= r;
  float bax = rf * (-k1y) - 0, bay = rf * k1x - 1;
  float hh = clampf((x * bax + y * bay) / (bax * bax + bay * bay), 0, r);
  float dx = x - bax * hh, dy = y - bay * hh;
  float s = (y * bax - x * bay) > 0 ? 1.f : -1.f;
  return len2(dx, dy) * s;
}
// heart of size s, centred (screen coords)
static inline float sdHeart(float x, float y, float s) {
  x = fabsf(x) / s; y = (-y / s) + 0.55f;  // to iq space (y up, tip at 0)
  float d;
  if (y + x > 1.0f) d = len2(x - 0.25f, y - 0.75f) - 0.35355339f;
  else {
    float m = 0.5f * fmaxf(x + y, 0.f);
    float a = (x) * (x) + (y - 1) * (y - 1), b = (x - m) * (x - m) + (y - m) * (y - m);
    d = sqrtf(fminf(a, b)) * ((x - y) > 0 ? 1.f : -1.f);
  }
  return d * s;
}
// arc band: circle radius ra, thickness rb, symmetric around +y (screen down), half-aperture ap (rad)
static inline float sdArc(float x, float y, float ap, float ra, float rb) {
  float scx = sinf(ap), scy = cosf(ap);
  x = fabsf(x);
  return ((scy * x > scx * y) ? len2(x - scx * ra, y - scy * ra) : fabsf(len2(x, y) - ra)) - rb;
}
static inline float sdSegment(float px, float py, float ax, float ay, float bx, float by) {
  float pax = px - ax, pay = py - ay, bax = bx - ax, bay = by - ay;
  float h = clampf((pax * bax + pay * bay) / (bax * bax + bay * bay), 0, 1);
  return len2(pax - bax * h, pay - bay * h);
}
// 4-point sparkle (astroid-like), size s
static inline float sdSparkle(float x, float y, float s) {
  x = fabsf(x) / s; y = fabsf(y) / s;
  float v = sqrtf(x) + sqrtf(y);
  return (v - 1.0f) * s * 0.5f;
}

// ---------- drawing primitives ----------
static inline float cov(float d) { return sat(0.5f - d); }  // 1px anti-aliased edge

void fillCircle(Canvas &cv, float cx, float cy, float r, Col c, float alpha = 1);
void fillEllipse(Canvas &cv, float cx, float cy, float rx, float ry, float ang, Col c, float alpha = 1);
void drawSparkle(Canvas &cv, float cx, float cy, float s, Col c, float alpha = 1);
void drawArc(Canvas &cv, float cx, float cy, float ra, float th, float ap, bool down, Col c, float alpha = 1);
void drawSegment(Canvas &cv, float ax, float ay, float bx, float by, float th, Col c, float alpha = 1);
void glowCircle(Canvas &cv, float cx, float cy, float r, float spread, Col c, float strength);
