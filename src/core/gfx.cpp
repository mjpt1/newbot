#if defined(__XTENSA__)
#pragma GCC optimize("fast-math")  // hardware float divide/sqrt instead of slow library calls
#endif
#include "gfx.h"

Rng rng;

void fillCircle(Canvas &cv, float cx, float cy, float r, Col c, float alpha) {
  cv.shade(cx - r - 1, cy - r - 1, cx + r + 1, cy + r + 1, [&](float x, float y, Col &o, float &a) {
    a = cov(len2(x - cx, y - cy) - r) * alpha; o = c; return a > 0;
  });
}

void fillEllipse(Canvas &cv, float cx, float cy, float rx, float ry, float ang, Col c, float alpha) {
  float R = (rx > ry ? rx : ry) + 1, ca = cosf(ang), sa = sinf(ang);
  cv.shade(cx - R, cy - R, cx + R, cy + R, [&](float x, float y, Col &o, float &a) {
    float dx = x - cx, dy = y - cy, u = dx * ca + dy * sa, v = -dx * sa + dy * ca;
    a = cov(sdEllipse(u, v, rx, ry)) * alpha; o = c; return a > 0;
  });
}

void drawSparkle(Canvas &cv, float cx, float cy, float s, Col c, float alpha) {
  if (s < 0.5f) return;
  cv.shade(cx - s - 1, cy - s - 1, cx + s + 1, cy + s + 1, [&](float x, float y, Col &o, float &a) {
    float d = sdSparkle(x - cx, y - cy, s);
    a = cov(d) * alpha;
    // soft halo around the sparkle
    float halo = 0.35f * alpha / (1 + len2(x - cx, y - cy) * len2(x - cx, y - cy) / (s * s * 0.08f));
    if (halo > a) a = halo;
    o = c; return a > 0;
  });
}

void drawArc(Canvas &cv, float cx, float cy, float ra, float th, float ap, bool down, Col c, float alpha) {
  float R = ra + th + 1;
  cv.shade(cx - R, cy - R, cx + R, cy + R, [&](float x, float y, Col &o, float &a) {
    float dy = y - cy; if (!down) dy = -dy;
    a = cov(sdArc(x - cx, dy, ap, ra, th * 0.5f)) * alpha; o = c; return a > 0;
  });
}

void drawSegment(Canvas &cv, float ax, float ay, float bx, float by, float th, Col c, float alpha) {
  float r = th * 0.5f + 1;
  cv.shade(fminf(ax, bx) - r, fminf(ay, by) - r, fmaxf(ax, bx) + r, fmaxf(ay, by) + r,
           [&](float x, float y, Col &o, float &a) {
             a = cov(sdSegment(x, y, ax, ay, bx, by) - th * 0.5f) * alpha; o = c; return a > 0;
           });
}

void glowCircle(Canvas &cv, float cx, float cy, float r, float spread, Col c, float strength) {
  float R = r + spread;
  cv.shade(cx - R, cy - R, cx + R, cy + R, [&](float x, float y, Col &o, float &a) {
    float d = len2(x - cx, y - cy) - r;
    if (d < 0) d = 0;
    float k = d / spread;
    a = strength * (1 - k) * (1 - k);
    o = c; return a > 0;
  });
}
