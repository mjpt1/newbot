// Liquid blob stages of the boot animation (grow + split). Kept in their own file so they can
// use fast-math (boot.cpp can't: the xtensa compiler crashes on it there).
#if defined(__XTENSA__)
#pragma GCC optimize("fast-math")
#endif
#include "boot.h"

using namespace bootT;
static const float CX = 120, CY = 120;

static inline float tableR(const float *tab, int n, float ang) {
  float f = (ang + kPI) / (2 * kPI) * n;
  int i = (int)f; float fr = f - i;
  i %= n; if (i < 0) i += n;
  return tab[i] + (tab[(i + 1) % n] - tab[i]) * fr;
}

void BootAnim::renderGrow(Canvas &cv) const {
  cv.clearBlack();
  float p = (t - BLACK) / (GROW - BLACK);
  float R = lerpf(3, 92, powf(p, 1.6f));
  if (p < 0.12f) {
    glowCircle(cv, CX, CY, R, 10, C(255, 255, 255), 0.25f);
    fillCircle(cv, CX, CY, R, C(255, 255, 255));
    return;
  }
  const Col beigeE = C(205, 180, 165), beige = C(230, 212, 198), white = C(252, 250, 247);
  float Rm = R * 1.3f + 4;
  cv.shadeA(CX - Rm, CY - Rm, CX + Rm, CY + Rm, [&](float x, float y, Col &o, float &a, float &e) {
    float dx = x - CX, dy = y - CY, r = len2(dx, dy), ang = fastAtan2(dy, dx);
    float ro = tableR(rOut, NA, ang), ri = tableR(rIn, NA, ang);
    float d = r - ro;
    e = fminf(fabsf(d), fabsf(r - ri));
    if (d > 0.5f) return false;
    o = mixc(beige, beigeE, smoothstepf(-14, 0, d));
    o = mixc(o, white, cov(r - ri) * (0.6f + 0.4f * smoothstepf(ri, 0, r)));
    a = cov(d);
    return a > 0;
  });
  // the two black dots (future pupils) float inside the liquid
  float dr = lerpf(1.5f, 7, sat((p - 0.12f) / 0.6f));
  for (int s = -1; s <= 1; s += 2)
    fillCircle(cv, CX + s * 22 + 3 * sinf(t * 1.3f + s), CY + s * 4 + 3 * sinf(t * 1.7f + 2 * s), dr, C(12, 12, 16));
}

// pupil dot position during the split: from inside the blob to the final eye centre
static inline void dotPos(float p, float t, int s, float &x, float &y) {
  float k = easeInOut(sat(p / 0.85f));
  x = lerpf(CX + s * 22 + 3 * sinf(t * 1.3f + s), CX + s * 55, k);
  y = lerpf(CY + s * 4 + 3 * sinf(t * 1.7f + 2 * s), CY, k);
}

void BootAnim::renderSplit(Canvas &cv) const {
  cv.clearBlack();
  float p = (t - GROW) / (SPLIT - GROW);
  float sep = easeInOut((p - 0.45f) / 0.55f) * 11;
  float reach = 92 * 1.25f * (1 - easeInOut(p / 0.6f)) - 4;
  const float hole = 0;  // old dimple effect off: the pupil dots carry through instead
  const float eyeK = easeInOut(sat((p - 0.72f) / 0.28f));  // liquid halves turn into eyes
  const float fade = 1 - eyeK;
  const Col beigeE = C(205, 180, 165), beige = C(230, 212, 198), white = C(252, 250, 247);
  float tt = t;
  float minO = 1e9f, minI = 1e9f, maxI = 0;
  for (int i = 0; i < NA; i++) { minO = fminf(minO, rOut[i]); minI = fminf(minI, rIn[i]); maxI = fmaxf(maxI, rIn[i]); }
  const Col deepWhite = mixc(beige, white, 0.85f);
  float crackRow[240];  // crack x position per row (sinf once per row, not per pixel)
  for (int r = 0; r < 240; r++) crackRow[r] = CX + 7 * sinf((r + 0.5f) * 0.045f + tt * 2) * (1 - sep / 13);
  cv.shadeA(0, 0, 240, 240, [&](float x, float y, Col &o, float &a, float &e) {
    int ri0 = (int)y; if (ri0 < 0) ri0 = 0; if (ri0 > 239) ri0 = 239;
    float crackX = crackRow[ri0];
    int side = x < crackX ? -1 : 1;
    float bx = CX + side * sep;
    float dx = x - bx, dy = y - CY, r = len2(dx, dy);
    float w = 1.5f + 0.45f * sep;
    e = fabsf(x - crackX) - w - 1;  // near the crack: shade per pixel
    if (r > 125) { e = fminf(e, r - 125); return false; }
    float ix = x - (bx + side * 22), ri = len2(ix, dy);
    // deep inside: colour is constant, no angle lookup needed
    if (r < minO - 15 && (ri < minI - 3 || ri > maxI + 3)) {
      e = fminf(e, fminf(minO - 15 - r, ri < minI - 3 ? minI - 3 - ri : ri - maxI - 3) + 2);
      o = ri < minI - 3 ? deepWhite : beige;
      a = 1;
      float act = sat((fabsf(y - CY) - reach) / 3);
      a *= 1 - act * sat(w - fabsf(x - crackX) + 0.5f);
      if (hole > 0.3f) {
        float h1 = len2(x - CX, y - (CY - 40)), h2 = len2(x - CX, y - (CY + 48));
        e = fminf(e, fminf(h1, h2) - hole);
        a *= sat(h1 - hole + 0.5f) * sat(h2 - hole + 0.5f);
      }
      a *= fade;
      return a > 0;
    }
    float ang = fastAtan2(dy, dx);
    float ro = tableR(rOut, NA, ang);
    float d = r - ro;
    e = fminf(e, fabsf(d));
    if (d > 0.5f) return false;
    float riR = tableR(rIn, NA, ang);  // reuse outer angle: one atan2 less
    e = fminf(e, fabsf(ri - riR));
    if (hole > 0.3f) e = fminf(e, fminf(len2(x - CX, y - (CY - 40)), len2(x - CX, y - (CY + 48))) - hole);
    o = mixc(beige, beigeE, smoothstepf(-14, 0, d));
    o = mixc(o, white, cov(ri - riR) * 0.85f);
    a = cov(d);
    // the dividing crack
    float act = sat((fabsf(y - CY) - reach) / 3);
    a *= 1 - act * sat(w - fabsf(x - crackX) + 0.5f);
    if (hole > 0.3f) {
      a *= sat(len2(x - CX, y - (CY - 40)) - hole + 0.5f);
      a *= sat(len2(x - CX, y - (CY + 48)) - hole + 0.5f);
    }
    a *= fade;
    return a > 0;
  });
  // white eye shapes grow in as the liquid fades
  if (eyeK > 0.001f) {
    const float rx = 50 * lerpf(0.7f, 1, eyeK), ry = 56 * lerpf(0.7f, 1, eyeK);
    const Col eyeW = C(250, 252, 253);
    for (int s = -1; s <= 1; s += 2) {
      float ex = CX + s * 55, ey = CY;
      cv.shadeA(ex - rx - 2, ey - ry - 2, ex + rx + 2, ey + ry + 2, [&](float x, float y, Col &o, float &a, float &e) {
        float u = (x - ex) / rx, v = (y - ey) / ry;
        float d = (sqrtf(u * u + v * v) - 1) * fminf(rx, ry);
        e = fabsf(d);
        o = eyeW; a = cov(d) * eyeK;
        return a > 0;
      });
    }
  }
  // the pupil dots ride along and end up in the middle of each eye
  for (int s = -1; s <= 1; s += 2) {
    float x, y; dotPos(p, tt, s, x, y);
    float r = lerpf(7, 9.5f, eyeK);
    fillCircle(cv, x, y, r, C(12, 12, 16));
  }
}

