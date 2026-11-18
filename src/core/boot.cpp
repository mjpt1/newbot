// GCC 8.4 (xtensa) crashes on one function here at -O2; -O1 is fine.
#if defined(__XTENSA__)
#pragma GCC optimize("O1")
#endif
#include "boot.h"

using namespace bootT;

static const float CX = 120, CY = 120;

void BootAnim::reset() {
  t = 0;
  mx = CX + 104; my = CY + 30;  // enters from the right edge
  mvx = -150; mvy = 20; curl = 0.6f; squash = 0; squashAng = 0; nextDart = 1.9f;
  cur = prev = 11; slideDir = 0; changedAt = 0; nextAt = CLOSED;
  for (auto &s : sparks) { s.born = -10; s.life = 0; }
  for (int i = 0; i < NA; i++) rOut[i] = rIn[i] = 0;
}

// ---------------- microbe ----------------
void BootAnim::updateMicrobe(float dt) {
  if (t > 9.3f) {  // ending: glide to the centre and hold still, then it swells (see render)
    float k = 1 - expf(-dt * 5.0f);
    mx += (CX - mx) * k; my += (CY - my) * k;
    mvx *= 1 - k; mvy *= 1 - k; squash *= 1 - k;
    return;
  }
  if (t > nextDart) {
    float tx, ty;
    if (rng.i(100) < 35) {  // aim past the edge: it will hit the wall and bounce
      float a = rng.f(0, 2 * kPI);
      tx = CX + cosf(a) * 160; ty = CY + sinf(a) * 160;
    } else {  // anywhere on the screen, but far enough to be a real trip
      for (int k = 0; k < 12; k++) {
        float a = rng.f(0, 2 * kPI), r = 100 * sqrtf(rng.f(0, 1));
        tx = CX + cosf(a) * r; ty = CY + sinf(a) * r;
        if (len2(tx - mx, ty - my) > 75) break;
      }
    }
    const float fr = 3.0f;  // with friction fr the dot glides distance = v / fr
    mvx = (tx - mx) * fr; mvy = (ty - my) * fr;
    float sp = len2(mvx, mvy);
    if (sp > 480) { mvx *= 480 / sp; mvy *= 480 / sp; }
    curl = rng.f(-1.4f, 1.4f);  // curved, organic paths
    nextDart = t + rng.f(0.55f, 1.35f);
  }
  // curve the path a bit, apply friction, move
  float c = cosf(curl * dt), s = sinf(curl * dt);
  float vx = mvx * c - mvy * s, vy = mvx * s + mvy * c;
  float f = expf(-3.0f * dt);
  mvx = vx * f; mvy = vy * f;
  mx += mvx * dt; my += mvy * dt;

  const float wall = 113;
  float dx = mx - CX, dy = my - CY, d = len2(dx, dy);
  if (d > wall) {
    float nx = dx / d, ny = dy / d;
    mx = CX + nx * wall; my = CY + ny * wall;
    float vn = mvx * nx + mvy * ny;
    if (vn > 0) {
      mvx -= 1.75f * vn * nx; mvy -= 1.75f * vn * ny;
      squash = fminf(1.0f, vn / 260.0f);
      squashAng = atan2f(ny, nx);
    }
  }
  squash *= expf(-dt * 6.5f);
}

void BootAnim::renderMicrobe(Canvas &cv) const {
  cv.fill(C(250, 250, 252));
  if (t > 7.6f && t < 7.95f) return;  // briefly vanishes
  float base = 4.6f;
  float sp = len2(mvx, mvy);
  float stretch = fminf(1.1f, sp / 330.0f);  // motion blur: longer when fast
  float a, b, ang;
  if (squash > stretch * 0.6f) { a = base * (1 - 0.55f * squash); b = base * (1 + 1.3f * squash); ang = squashAng; }
  else { a = base * (1 + stretch); b = base * (1 - 0.3f * stretch); ang = atan2f(mvy, mvx); }
  float alpha = sat((t - FADE) / 0.4f);
  if (t >= 7.95f && t < 8.4f) alpha = (t - 7.95f) / 0.45f;
  Col col = C(82, 82, 108);
  if (t > 9.3f && t < 9.95f) { float k = sat((t - 9.3f) / 0.4f); a = b = base * (1 - 0.5f * k) + (a - base) * (1 - k); }
  else if (t >= 9.95f) {
    // swell: grows faster and faster (exponential) until the whole screen is black
    float r = base * 0.5f * expf((t - 9.95f) * 9.9f);
    if (r > 172) { cv.clearBlack(); return; }
    a = b = r;
    col = mixc(col, C(0, 0, 0), sat((r - 10) / 60));
    fillCircle(cv, mx, my, r, col, alpha);
    return;
  }
  // soft shadow + body + tiny highlight
  fillEllipse(cv, mx, my + 1.2f, a + 1.2f, b + 1.2f, ang, C(200, 200, 215), alpha * 0.5f);
  fillEllipse(cv, mx, my, a, b, ang, col, alpha * (1 - 0.4f * sat(stretch - 0.5f)));
  if (sp < 200) fillCircle(cv, mx - a * 0.3f, my - b * 0.35f, 1.1f, C(170, 170, 190), alpha * 0.8f);
}

// ---------------- blob ----------------
static float blobK(float th, float t, float seed) {
  return 0.13f * sinf(3 * th + t * 1.1f + seed) + 0.08f * sinf(5 * th - t * 1.6f + seed * 2) +
         0.06f * sinf(2 * th + t * 0.7f - seed);
}

// ---------------- slot machine eye styles ----------------
// generic shaded shape: rim colour at the edge -> core inside, soft glow, glossy highlight
#ifdef _MSC_VER
#define NOINLINE __declspec(noinline)
#else
#define NOINLINE __attribute__((noinline))
#endif
template <class SD>
NOINLINE static void shapeFill(Canvas &cv, float cx, float cy, float R, SD sd, Col core, Col rim, float glow,
                      float lidY = -1e9f, float lidSlope = 0) {
  const float gR = 10;
  auto f = [&](float x, float y, float d, Col &o, float &a) {
    float lid = sat(y - (lidY + lidSlope * (x - cx)) + 0.5f);
    if (lid <= 0 || d > gR) return false;
    if (d > 0.5f) { float k = 1 - d / gR; a = glow * k * k * k * lid; o = rim; return a > 0; }
    o = mixc(rim, core, smoothstepf(0, 15, -d));
    float hx = (x - (cx - R * 0.3f)) / R, hy = (y - (cy - R * 0.42f)) / R;
    o = mixc(o, C(255, 255, 255), smoothstepf(0.32f, 0.05f, sqrtf(hx * hx + hy * hy * 2)) * 0.5f);
    a = cov(d) * lid;
    return a > 0;
  };
  // one sample per 2x2 block; per-pixel only near the shape edge or the lid line
  int xa = ((int)floorf(cx - R - gR)) & ~1, xb = (int)ceilf(cx + R + gR);
  int ya = ((int)floorf(cy - R - gR)) & ~1, yb = (int)ceilf(cy + R + gR);
  if (xa < 0) xa = 0; if (xb > cv.w) xb = cv.w; if (ya < cv.y0) ya = cv.y0; if (yb > cv.y1) yb = cv.y1;
  for (int y = ya; y < yb; y += 2)
    for (int x = xa; x < xb; x += 2) {
      float px = x + 1.0f, py = y + 1.0f;
      float d = sd(px - cx, py - cy);
      if (d > gR + 2) continue;
      float ld = py - (lidY + lidSlope * (px - cx));
      Col c; float al;
      if (fabsf(d) > 2.5f && fabsf(ld) > 2.5f) {
        if (!f(px, py, d, c, al)) continue;
        for (int j = 0; j < 2 && y + j < yb; j++)
          for (int i = 0; i < 2 && x + i < xb; i++) cv.blend(x + i, y + j, c, al);
      } else {
        for (int j = 0; j < 2 && y + j < yb; j++)
          for (int i = 0; i < 2 && x + i < xb; i++) {
            float qx = x + i + 0.5f, qy = y + j + 0.5f;
            if (f(qx, qy, sd(qx - cx, qy - cy), c, al)) cv.blend(x + i, y + j, c, al);
          }
      }
    }
}

enum { S_WHITE_GREENBLUE, S_ORANGE_GLOW, S_ORANGE_RED, S_PINK_STAR, S_CYAN_LID, S_PURPLE_ANGRY,
       S_HEART, S_GREEN_RED, S_GOLD_STAR, S_SLEEPY_ORANGE, S_WHITE_CYAN, S_EMPTY, S_COUNT };

void BootAnim::renderStyle(Canvas &cv, int s, float ox) const {
  float L = CX - 45 + ox, Rr = CX + 45 + ox, Y = CY;
  auto oval = [](float rx, float ry) { return [=](float x, float y) { return sdEllipse(x, y, rx, ry); }; };
  auto both = [&](auto sd, float R, Col core, Col rim, float glow, float lidY = -1e9f, float sl = 0) {
    shapeFill(cv, L, Y, R, sd, core, rim, glow, lidY, sl);
    shapeFill(cv, Rr, Y, R, sd, core, rim, glow, lidY, -sl);
  };
  switch (s) {
    case S_WHITE_GREENBLUE:
      both(oval(38, 44), 44, C(255, 255, 255), C(200, 210, 220), 0.2f, Y - 26);
      shapeFill(cv, L + 5, Y + 4, 34, oval(25, 34), C(120, 255, 170), C(10, 170, 90), 0, Y - 26);
      shapeFill(cv, Rr + 5, Y + 4, 34, oval(25, 34), C(120, 190, 255), C(20, 90, 230), 0, Y - 26);
      break;
    case S_ORANGE_GLOW:
      both(oval(40, 50), 50, C(255, 236, 205), C(255, 150, 60), 0.4f);
      break;
    case S_ORANGE_RED:
      both(oval(40, 50), 50, C(255, 214, 170), C(245, 150, 90), 0.3f);
      shapeFill(cv, L + 6, Y + 12, 34, oval(14, 33), C(255, 90, 60), C(200, 20, 10), 0);
      shapeFill(cv, Rr + 6, Y + 12, 34, oval(14, 33), C(255, 90, 60), C(200, 20, 10), 0);
      break;
    case S_PINK_STAR: {
      float w = sinf(t * 7) * 0.12f;
      shapeFill(cv, L, Y, 46, [=](float x, float y) { float c = cosf(w), s2 = sinf(w); return sdStar5(x * c - y * s2, x * s2 + y * c, 44, 0.48f); }, C(255, 200, 240), C(230, 50, 190), 0.4f);
      shapeFill(cv, Rr, Y, 46, [=](float x, float y) { float c = cosf(-w), s2 = sinf(-w); return sdStar5(x * c - y * s2, x * s2 + y * c, 44, 0.48f); }, C(255, 200, 240), C(230, 50, 190), 0.4f);
      break;
    }
    case S_CYAN_LID:
      both(oval(38, 38), 40, C(255, 255, 255), C(30, 200, 230), 0.3f, Y - 14);
      break;
    case S_PURPLE_ANGRY:
      both(oval(40, 40), 42, C(230, 215, 255), C(140, 100, 245), 0.35f, Y - 22, 0.45f);
      break;
    case S_HEART: {
      float beat = 1 + 0.07f * powf(fabsf(sinf(t * 7)), 4);
      both([=](float x, float y) { return sdHeart(x, y, 40 * beat); }, 46, C(255, 225, 190), C(255, 120, 50), 0.4f);
      break;
    }
    case S_GREEN_RED:
      both(oval(40, 34), 40, C(230, 255, 190), C(150, 220, 80), 0.3f, Y - 22);
      shapeFill(cv, L, Y + 4, 12, [](float x, float y) { return len2(x, y) - 11; }, C(255, 110, 100), C(210, 20, 20), 0);
      shapeFill(cv, Rr, Y + 4, 12, [](float x, float y) { return len2(x, y) - 11; }, C(255, 110, 100), C(210, 20, 20), 0);
      break;
    case S_GOLD_STAR:
      both([](float x, float y) { return sdStar5(x, y, 44, 0.5f); }, 46, C(255, 248, 200), C(255, 170, 20), 0.45f);
      break;
    case S_SLEEPY_ORANGE:
      drawArc(cv, L, Y - 12, 26, 4, 1.15f, true, C(255, 130, 90));
      drawArc(cv, Rr, Y - 12, 26, 4, 1.15f, true, C(255, 130, 90));
      break;
    case S_WHITE_CYAN:
      both(oval(38, 44), 44, C(255, 255, 255), C(205, 215, 225), 0.2f);
      shapeFill(cv, L - 20, Y + 4, 30, oval(14, 30), C(140, 230, 255), C(20, 150, 230), 0);
      break;
    default: break;
  }
}

void BootAnim::renderSlot(Canvas &cv) const {
  cv.clearBlack();
  float ox = 0;
  if (slideDir) {
    float k = sat((t - changedAt) / 0.2f);
    ox = slideDir * (1 - easeOutCubic(k)) * 210;
    renderStyle(cv, prev, ox - slideDir * 210);
  }
  renderStyle(cv, cur, ox);
  for (const auto &s : sparks) {
    float p = (t - s.born) / s.life;
    if (p < 0 || p > 1) continue;
    drawSparkle(cv, s.x, s.y, 8 * sinf(p * kPI), C(255, 255, 255));
  }
}

// ---------------- timeline ----------------
void BootAnim::update(float dt) {
  t += dt;
  if (t >= FADE && t < MICROBE) updateMicrobe(dt);
  if (t >= BLACK && t < SPLIT) {  // wobbling blob outline
    float p = sat((t - BLACK) / (GROW - BLACK));
    float R = t < GROW ? lerpf(3, 92, powf(p, 1.6f)) : 92;
    float wob = sat(p * 3);
    float Ri = t < GROW ? R * 0.6f : 92 * 0.5f;
    for (int i = 0; i < NA; i++) {
      float th = -kPI + i * 2 * kPI / NA;
      rOut[i] = R * (1 + wob * blobK(th, t, 0));
      rIn[i] = Ri * (1 + wob * 1.3f * blobK(th, t * 1.3f, 2));
    }
  }
  if (t >= CLOSED && t < SLOT) {
    float p = (t - CLOSED) / (SLOT - CLOSED);
    if (t >= nextAt) {
      prev = cur;
      do { cur = rng.i(S_COUNT); } while (cur == prev);
      changedAt = t;
      slideDir = rng.i(10) < 4 ? (rng.i(2) ? 1 : -1) : 0;
      nextAt = t + 0.17f + 0.55f * p * p;  // spins fast, then slows down
    }
    for (auto &s : sparks)
      if (t > s.born + s.life) {
        float a = rng.f(0, 2 * kPI), r = rng.f(35, 105);
        s.x = CX + cosf(a) * r; s.y = CY + sinf(a) * r;
        s.born = t + rng.f(0, 0.5f); s.life = rng.f(0.35f, 0.8f);
      }
  }
}

void BootAnim::render(Canvas &cv) const {
  if (t < FADE) {
    float p = t / FADE;
    Col c = p < 0.33f ? mixc(C(5, 5, 25), C(70, 35, 95), p / 0.33f)
          : p < 0.66f ? mixc(C(70, 35, 95), C(205, 140, 160), (p - 0.33f) / 0.33f)
                      : mixc(C(205, 140, 160), C(250, 250, 252), (p - 0.66f) / 0.34f);
    cv.fill(c);
  } else if (t < MICROBE) renderMicrobe(cv);
  else if (t < BLACK) cv.clearBlack();
  else if (t < GROW) renderGrow(cv);
  else if (t < SPLIT) renderSplit(cv);
  else if (t < CLOSED) {
    cv.clearBlack();
    float a = sat((t - SPLIT) / 0.15f);
    drawArc(cv, CX - 45, CY + 32, 42, 3, 1.1f, false, C(255, 255, 255), a);
    drawArc(cv, CX + 45, CY + 32, 42, 3, 1.1f, false, C(255, 255, 255), a);
  } else if (t < SLOT) renderSlot(cv);
  else {  // sleeping mint eyes until the face takes over
    cv.clearBlack();
    float a = sat((t - SLOT) / 0.3f);
    for (int s = -1; s <= 1; s += 2) {
      drawArc(cv, CX + s * 55, CY - 13, 46, 6, 1.05f, true, hsv(160, 0.15f, 0.9f), 0.25f * a);
      drawArc(cv, CX + s * 55, CY - 13, 46, 4, 1.0f, true, C(240, 246, 248), a);
    }
  }
}
