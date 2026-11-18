#if defined(__XTENSA__)
#pragma GCC optimize("fast-math")  // hardware float divide/sqrt instead of slow library calls
#endif
#include "face.h"

const char *const kEmotionName[E_COUNT] = {"neutral", "happy", "sad", "angry", "surprised", "sleepy",
                                           "dizzy", "love", "curious", "wink", "asleep",
                                           "swear", "eat", "vomit"};

static const float EYE_RX = 50, EYE_RY = 56, EYE_SPREAD = 55, EYE_Y = 120;

void Face::setEmotion(Emotion e) {
  if (e != emo) { blink(); emoT = t; }  // a blink hides the change and feels alive
  emo = e;
  float oL = 1, oR = 1, ti = 0, ha = 0, sL = 1, sR = 1, pu = 5, yo = 0, lo = 0, di = 0;
  switch (e) {
    case E_NEUTRAL: break;
    case E_HAPPY: ha = 0.5f; sL = sR = 1.03f; break;
    case E_SAD: oL = oR = 0.72f; ti = -0.55f; yo = 7; pu = 6; break;
    case E_ANGRY: oL = oR = 0.78f; ti = 0.6f; sL = sR = 0.96f; pu = 4; break;
    case E_SURPRISED: sL = sR = 1.13f; pu = 3.2f; yo = -3; break;
    case E_SLEEPY: oL = oR = 0.36f; ti = -0.12f; yo = 5; break;
    case E_DIZZY: di = 1; oL = 0.92f; oR = 0.85f; break;
    case E_LOVE: lo = 1; ha = 0.25f; break;
    case E_CURIOUS: sL = 1.12f; sR = 0.9f; oR = 0.78f; ti = -0.1f; break;
    case E_WINK: oL = 0; ha = 0.45f; break;
    case E_ASLEEP: oL = oR = 0; yo = 6; break;
    case E_SWEAR: oL = oR = 0.62f; ti = 0.85f; sL = sR = 0.94f; pu = 3.6f; yo = -4; break;
    case E_EAT: ha = 0.55f; yo = -10; sL = sR = 0.92f; break;
    case E_VOMIT: oL = 0.5f; oR = 0.42f; ti = -0.45f; yo = -12; sL = sR = 0.9f; pu = 4; break;
    default: break;
  }
  openL.set(oL); openR.set(oR); tilt.set(ti); happy.set(ha);
  scaleL.set(sL); scaleR.set(sR); pupil.set(pu); yoff.set(yo); love.set(lo); dizzy.set(di);
  rage.set(e == E_ANGRY || e == E_SWEAR ? 1 : 0);
  if (e == E_SURPRISED) { scaleL.kick(1.2f); scaleR.kick(1.2f); }  // little pop
}

void Face::blink() { if (t - blinkT > 0.3f) blinkT = t; }

void Face::startClosed() { openL.snap(0); openR.snap(0); }

void Face::lookAt(float x, float y, float hold) { lookX = x; lookY = y; holdUntil = t + hold; nextLook = t + hold; }

float Face::blinkMul() const {
  float b = t - blinkT;
  if (b < 0 || b > 0.24f) return 1;
  if (b < 0.075f) { float k = b / 0.075f; return 1 - k * k; }       // close: accelerate
  if (b < 0.095f) return 0;                                             // shut
  return easeOutCubic((b - 0.095f) / 0.145f);                           // open: decelerate
}

void Face::update(float dt) {
  t += dt;
  // autonomous looking around
  if (autoLook && t > nextLook) {
    if (rng.i(10) < 3) { lookX = 0; lookY = 0; }
    else { lookX = rng.f(-0.9f, 0.9f); lookY = rng.f(-0.7f, 0.7f); }
    nextLook = t + rng.f(0.9f, 3.4f);
    if (rng.i(10) < 3) blink();  // people often blink with big eye moves
  }
  if (t > nextMicro) {  // tiny fixation jitter keeps the eyes alive
    microX = rng.f(-0.035f, 0.035f); microY = rng.f(-0.03f, 0.03f);
    nextMicro = t + rng.f(0.25f, 0.8f);
  }
  if (t > nextBlink) {
    blink();
    nextBlink = t + rng.f(2.2f, 5.5f);
    if (rng.i(6) == 0) nextBlink = t + 0.32f;  // double blink
  }
  float tx = clampf(lookX + biasX + microX, -1.1f, 1.1f);
  float ty = clampf(lookY + biasY + microY, -1.0f, 1.0f);
  if (emo == E_ASLEEP) { tx = 0; ty = 0.3f; }
  const float et = t - emoT;
  if (emo == E_ANGRY || emo == E_SWEAR) {  // trembling with rage
    float k = emo == E_SWEAR ? 0.06f : 0.03f;
    tx += k * sinf(t * 47) ; ty += k * 0.6f * sinf(t * 53 + 1);
  }
  if (emo == E_EAT) { tx = 0; ty = 0.55f; yoff.set(-10 + 4 * fabsf(sinf(et * 9))); }  // look at food, chew
  if (emo == E_VOMIT) {
    float heave = fmodf(et, 2.4f);  // 0-0.7 retch, 0.7-1.9 spew, 1.9-2.4 drool
    if (heave < 0.7f) {             // retch: eyes squeeze shut, face lurches up and trembles
      float k = sinf(heave / 0.7f * kPI);
      openL.set(0.18f); openR.set(0.14f); tilt.set(-0.7f);
      yoff.set(-22 - 8 * k); tx = 0.08f * sinf(t * 41); ty = -0.5f;
    } else if (heave < 1.9f) {      // spew: eyes half open, shaking (eyes stay high, mouth needs the room)
      openL.set(0.45f); openR.set(0.38f); tilt.set(-0.5f);
      yoff.set(-26 + 2 * sinf(t * 30)); tx = 0.04f * sinf(t * 29); ty = -0.35f;
    } else {                        // after: dazed
      openL.set(0.5f); openR.set(0.42f); tilt.set(-0.45f);
      yoff.set(-24); tx = 0; ty = -0.3f;
    }
  }
  // slow idle sway, like a creature shifting its weight (two incommensurate sines = never repeats)
  tx += 0.07f * sinf(t * 0.41f) + 0.035f * sinf(t * 0.97f + 1.3f);
  ty += 0.05f * sinf(t * 0.53f + 0.7f) + 0.025f * sinf(t * 1.21f);
  gx.set(tx); gy.set(ty);

  Spring *all[] = {&hue, &tint, &gx, &gy, &openL, &openR, &tilt, &happy, &scaleL, &scaleR,
                   &pupil, &yoff, &love, &dizzy, &rage, &faceScale, &faceY};
  for (Spring *s : all) s->step(dt);
}

void Face::renderEye(Canvas &cv, int side) const {
  const bool left = side < 0;
  const float fs = faceScale;
  float sc = fs * (left ? scaleL.x : scaleR.x);
  if (love > 0.01f) sc *= 1 + 0.05f * love * powf(fabsf(sinf(t * 3.2f)), 6);  // heartbeat
  // breathing: a slow, barely visible swell (asleep breathes slower and deeper)
  const float br = emo == E_ASLEEP ? sinf(t * 1.1f) * 1.6f : sinf(t * 1.7f);
  sc *= 1 + 0.014f * br;
  // blink squash: the eye widens a little and drops as the lid closes
  const float bsq = 1 - blinkMul();
  const float g_x = gx, g_y = gy;

  // StarBoy-style 3D: the face is painted on a ball. Looking around turns the ball, so each
  // eye slides along the sphere and gets foreshortened (narrower) as it nears the rim.
  const float RB = 114 * fs;                                   // ball radius on screen
  const float th0 = asinf(clampf(side * EYE_SPREAD * fs / RB, -0.95f, 0.95f));
  const float ph0 = asinf(clampf((EYE_Y + faceY + yoff - 120) / RB, -0.95f, 0.95f));
  const float yaw = g_x * 0.36f, pitch = g_y * 0.28f;
  const float thx = th0 + yaw, phy = ph0 + pitch;
  const float sx = cosf(thx), sy = cosf(phy);                  // foreshortening
  // squash & stretch while the ball turns fast (smear in the direction of motion)
  float sp = clampf(fabsf(gx.v) * 0.012f + fabsf(gy.v) * 0.008f, 0, 0.10f);
  float rx = EYE_RX * sc * fmaxf(0.35f, sx) * (1 + sp) * (1 + 0.07f * bsq);
  float ry = EYE_RY * sc * fmaxf(0.45f, sy) * (1 - sp * 0.5f);
  float ex = 120 + RB * sinf(thx) * (0.92f + 0.08f * sy);
  float ey = 120 + RB * sinf(phy) + 1.2f * br + 3.0f * bsq;
  float ang = 0.10f * side + g_x * 0.04f;
  if (dizzy > 0.01f) ang += sinf(t * 5.5f + side) * 0.18f * dizzy;

  float open = (left ? openL.x : openR.x) * blinkMul();
  float hp = happy;

  // white by default; tint > 0 colours the eye with hue
  const float tn = tint;
  Col core = hsv(hue, 0.04f + 0.16f * tn, 1.0f);
  const Col mid = hsv(hue, 0.07f + 0.33f * tn, 0.97f);
  Col rim = hsv(hue + 6, 0.10f + 0.68f * tn, 0.80f);
  Col glowC = hsv(hue, 0.15f + 0.65f * tn, 0.9f);
  const float rg = rage;
  if (rg > 0.01f) {  // fire: hot yellow-orange centre, deep red rim, flickering
    float fl = 0.9f + 0.1f * sinf(t * 23 + side) * sinf(t * 37);
    core = mixc(core, mulc(C(255, 170, 60), fl), rg);
    rim = mixc(rim, C(200, 20, 10), rg);
    glowC = mixc(glowC, C(255, 60, 20), rg);
  }
  Col pupilC = tn > 0.3f ? hsv(hue + 5, 0.95f, 0.30f) : C(14, 18, 26);
  const Col white = C(255, 255, 255);
  Col loveC = C(255, 70, 140);

  // fully (or nearly) shut: draw a glowing lid line instead
  if (open < 0.10f) {
    float a = sat(1 - open / 0.10f);
    bool smile = hp > 0.3f;  // happy closed eyes look like ^ ^
    float cy = smile ? ey + ry * 0.25f : ey - ry * 0.25f;
    drawArc(cv, ex, cy, rx * 0.92f, 5.5f * fs, 1.05f, !smile, glowC, 0.25f * a);  // soft glow pass
    drawArc(cv, ex, cy, rx * 0.92f, 4.0f * fs, 1.0f, !smile, core, a);
    return;
  }

  // ---- row-span renderer (fast on ESP32: no per-pixel sqrt/divide inside the eye) ----
  const float base = -1.04f + 2.1f * (1 - open);  // upper lid line in normalised v
  const float ti = tilt;
  const float irx = 1 / rx, iry = 1 / ry;
  const float rim0 = 0.40f, rimK = 1 / (1.04f - rim0);  // rim darkening on q^2
  const float lv = love;

  // pupil (eye-local pixels)
  float pr = pupil * sc * 1.9f;
  float pu = g_x * rx * 0.22f, pv = g_y * ry * 0.18f;
  if (dizzy > 0.01f) {
    float ph = t * 8.5f + (left ? 0 : kPI);
    pu = lerpf(pu, cosf(ph) * rx * 0.33f, dizzy);
    pv = lerpf(pv, sinf(ph) * ry * 0.30f, dizzy);
  }
  // sphere foreshortening of the pupil: squash along the radial direction near the edge
  const float pnu = pu * irx, pnv = pv * iry, prr = sqrtf(pnu * pnu + pnv * pnv);
  const float fk = sqrtf(fmaxf(0.25f, 1 - prr * prr)), ifk = 1 / fk;
  const float pcu = prr > 1e-3f ? pnu / prr : 1, pcv = prr > 1e-3f ? pnv / prr : 0;
  const float pcx = ex + pu, pcy = ey + pv;
  const float pBox = (lv > 0.5f ? pr * 2.8f : pr * 1.5f) + 2;

  int ya = (int)floorf(ey - ry - 2), yb = (int)ceilf(ey + ry + 2);
  if (ya < cv.y0) ya = cv.y0; if (yb > cv.y1) yb = cv.y1;
  for (int yi = ya; yi < yb; yi++) {
    const float y = yi + 0.5f, v = y - ey, nv = v * iry;
    const float k = 1 - nv * nv;
    if (k <= -0.05f) continue;
    const float half = rx * sqrtf(fmaxf(0, k));       // one sqrt per row
    // vertical light falloff and lid-independent row colour
    const float light = 0.90f + 0.16f * (0.7f * sqrtf(fmaxf(0, k)) - nv * 0.25f);
    Col rowC = mulc(core, light), rowRim = mulc(rim, light);
    if (lv > 0.01f) { rowC = mixc(rowC, mulc(C(255, 200, 225), light), lv * 0.85f); rowRim = mixc(rowRim, mulc(C(240, 60, 130), light), lv * 0.85f); }
    // edge slope for anti-aliasing: |d(x-span)/dy| makes top/bottom rows soft too
    const float slope = fabsf(nv) * rx * iry / fmaxf(0.08f, sqrtf(fmaxf(0, k)));
    const float aaW = 1 + slope;  // edge softness in px
    int xa = (int)floorf(ex - half - aaW - 1), xb = (int)ceilf(ex + half + aaW + 1);
    if (xa < 0) xa = 0; if (xb > cv.w) xb = cv.w;
    uint16_t *row = cv.buf + yi * cv.w;
    for (int xi = xa; xi < xb; xi++) {
      const float x = xi + 0.5f, u = x - ex, nu = u * irx;
      // body coverage from the row span
      float ed = half - fabsf(u);
      float body = sat(ed / aaW + 0.5f);
      if (k < 0) body *= sat(1 + k * 20);
      if (body <= 0) continue;
      // lids (cheap: linear in nu / nv)
      float inner = -side * nu;
      float lidV = base + ti * inner * 0.62f + 0.10f * nu * nu * (open < 0.98f ? 1 : 0);
      float lowV = 1.06f - hp * (1.25f - 0.75f * nu * nu);
      float lidD = (nv - lidV) * ry, lowD = (lowV - nv) * ry;
      float mask = sat(lidD + 0.5f) * sat(lowD + 0.5f);
      if (mask <= 0) continue;
      // colour: flat core, darker toward the rim (sphere), shadow under lids
      float q2 = nu * nu + nv * nv;
      Col c = q2 <= rim0 ? rowC : mixc(rowC, rowRim, sat((q2 - rim0) * rimK));
      float sh = 1;
      if (lidV > -1.0f && lidD < 10) sh -= 0.42f * (1 - lidD * 0.1f);
      if (lowV < 1.0f && lowD < 9) sh *= 1 - 0.30f * (1 - lowD * (1 / 9.f));
      if (sh < 1) c = mulc(c, sh);
      // pupil (only inside its small box)
      float du = x - pcx, dv = y - pcy;
      if (fabsf(du) < pBox && fabsf(dv) < pBox) {
        float pd;
        if (lv > 0.5f) pd = sdHeart(du, dv, pr * 2.6f);
        else {
          // tall oval pupil, squashed by the same sphere foreshortening as the eye
          float prx = pr * fmaxf(0.35f, sx), pry = pr * 1.45f * fmaxf(0.45f, sy);
          pd = (len2(du / prx, dv / pry) - 1) * fminf(prx, pry);
        }
        float pc = cov(pd);
        if (pc > 0) {
          c = mixc(c, lv > 0.5f ? loveC : pupilC, pc);
          float cl = len2(du + pr * 0.38f, dv + pr * 0.42f) - pr * 0.32f;  // catchlight
          c = mixc(c, white, cov(cl) * 0.75f);
        }
      }
      float a = body * mask;
      if (a >= 0.996f) row[xi] = cv.pack(xi, yi, c);
      else cv.blend(xi, yi, c, a);
    }
  }
  // glass highlights: soft big one top-left + small sharp one (small areas, separate passes)
  if (open > 0.35f) {
    float hx = ex - rx * (0.34f - g_x * 0.08f), hy = ey - ry * (0.50f - g_y * 0.08f);
    float hr = rx * 0.26f;
    float lidY = ey + ry * (base + fabsf(ti) * 0.62f + 0.05f);
    cv.shade(hx - hr, fmaxf(hy - hr, lidY), hx + hr, hy + hr, [&](float x, float y, Col &o, float &a) {
      float dx = (x - hx) / hr, dy = (y - hy) * 1.45f / hr;
      float d2 = dx * dx + dy * dy;
      if (d2 >= 1) return false;
      a = 0.55f * (1 - d2) * (1 - d2) * sat((y - lidY) * 0.3f);
      o = white; return a > 0;
    });
    fillCircle(cv, ex + rx * 0.34f, ey + ry * 0.44f, 3.2f * sc, white, 0.45f * sat((ey + ry * 0.44f - lidY) * 0.2f));
  }
}

static void drawZ(Canvas &cv, float x, float y, float s, Col c, float a) {
  float th = s * 0.22f;
  drawSegment(cv, x - s, y - s, x + s, y - s, th, c, a);
  drawSegment(cv, x + s, y - s, x - s, y + s, th, c, a);
  drawSegment(cv, x - s, y + s, x + s, y + s, th, c, a);
}

void Face::render(Canvas &cv) const {
  renderEye(cv, -1);
  renderEye(cv, 1);
  if (emo == E_ASLEEP) {  // floating Zzz
    for (int i = 0; i < 3; i++) {
      float p = fmodf(t * 0.35f + i / 3.0f, 1.0f);
      float a = sinf(p * kPI);
      drawZ(cv, 168 + p * 30 + sinf(p * 9) * 4, 92 - p * 60, 5 + p * 7, hsv(hue, 0.4f, 1.0f), a * 0.9f);
    }
  }
  const float et = t - emoT;
  if (rage > 0.05f) {  // flames licking up from the top of each eye
    const float rg = rage;
    for (int s = -1; s <= 1; s += 2) {
      float ex = 120 + s * 55 * faceScale + gx * 30, top = 120 + faceY + yoff - 34;
      // 5 flame tongues: each a wavy column of overlapping blobs, fat at the base, pointed at the tip
      for (int f = 0; f < 5; f++) {
        float bx = ex + (f - 2) * 15 * (1 - 0.12f * fabsf(f - 2.0f));
        float h = (26 + 12 * sinf(t * 7.3f + f * 1.9f + s) + 8 * sinf(t * 13.1f + f)) * rg
                  * (f == 2 ? 1.25f : (f == 1 || f == 3) ? 1.0f : 0.7f);
        float by = top + 8 + fabsf(f - 2.0f) * 4;
        for (int k = 0; k < 9; k++) {
          float p = k / 8.0f;
          float x = bx + sinf(t * 11 + f * 2.1f - p * 4) * 4.5f * p;
          float y = by - p * h;
          float r = (8.5f - 7 * p) * rg;
          Col c = p < 0.3f ? mixc(C(255, 240, 150), C(255, 160, 40), p / 0.3f)
                           : mixc(C(255, 160, 40), C(220, 30, 10), (p - 0.3f) / 0.7f);
          fillCircle(cv, x, y, r, c, rg * (1 - 0.55f * p));
        }
      }
    }
  }
  if (emo == E_ANGRY || emo == E_SWEAR) {  // throbbing anger vein, top right
    // classic anime "anger mark": four curved brackets around a centre, bulging outward, throbbing
    float s = 11 * (1 + 0.18f * powf(fabsf(sinf(t * 6)), 4));
    float vx = 184, vy = 46;
    Col red = C(255, 50, 40);
    for (int i = 0; i < 4; i++) {
      float a = i * kPI / 2 + kPI / 4;  // diagonal directions
      float cx = vx + cosf(a) * s, cy = vy + sinf(a) * s;
      float ux = cosf(a), uy = sinf(a), px = -uy, py = ux;  // outward and along
      float L = s * 0.5f;
      // bracket = a V whose tip points at the centre; the gaps between the four make the mark
      drawSegment(cv, cx + px * L + ux * 3, cy + py * L + uy * 3, cx - ux * 3, cy - uy * 3, 3.6f, red, 1);
      drawSegment(cv, cx - px * L + ux * 3, cy - py * L + uy * 3, cx - ux * 3, cy - uy * 3, 3.6f, red, 1);
    }
  }
  if (emo == E_SWEAR) {  // grawlix symbols popping out of the mouth area: # ! * @
    Col yel = C(255, 225, 70), red = C(255, 80, 60);
    for (int i = 0; i < 4; i++) {
      float p = fmodf(et * 0.9f + i * 0.25f, 1.0f);
      float a = sinf(p * kPI);
      float x = 120 + (i - 1.5f) * 34 + sinf(p * 7 + i) * 6, y = 222 - p * 46;
      float s = 7 + 3 * a;
      Col c = (i & 1) ? red : yel;
      switch (i) {
        case 0:  // #
          drawSegment(cv, x - s * 0.4f, y - s, x - s * 0.6f, y + s, 2.4f, c, a);
          drawSegment(cv, x + s * 0.6f, y - s, x + s * 0.4f, y + s, 2.4f, c, a);
          drawSegment(cv, x - s, y - s * 0.35f, x + s, y - s * 0.35f, 2.4f, c, a);
          drawSegment(cv, x - s, y + s * 0.35f, x + s, y + s * 0.35f, 2.4f, c, a);
          break;
        case 1:  // !
          drawSegment(cv, x, y - s, x, y + s * 0.35f, 3.2f, c, a);
          fillCircle(cv, x, y + s * 0.9f, 2.2f, c, a);
          break;
        case 2:  // *
          for (int k = 0; k < 3; k++) {
            float b = k * kPI / 3 + p * 3;
            drawSegment(cv, x - cosf(b) * s, y - sinf(b) * s, x + cosf(b) * s, y + sinf(b) * s, 2.4f, c, a);
          }
          break;
        default:  // lightning bolt
          drawSegment(cv, x + s * 0.4f, y - s, x - s * 0.3f, y, 2.6f, c, a);
          drawSegment(cv, x - s * 0.3f, y, x + s * 0.3f, y, 2.6f, c, a);
          drawSegment(cv, x + s * 0.3f, y, x - s * 0.4f, y + s, 2.6f, c, a);
          break;
      }
    }
  }
  if (emo == E_EAT) {  // a cookie rises, gets bitten every chew, then a new one
    float cyc = fmodf(et, 3.0f);
    int bites = (int)(cyc / 0.55f);
    float cx = 120, cy = 206 - 8 * sat(cyc * 3) , r = 24;
    if (bites < 5) {
      fillCircle(cv, cx, cy, r, C(200, 140, 70));
      fillCircle(cv, cx - 9, cy - 6, 3, C(80, 45, 25));   // chocolate chips
      fillCircle(cv, cx + 8, cy + 4, 3.2f, C(80, 45, 25));
      fillCircle(cv, cx - 2, cy + 11, 2.6f, C(80, 45, 25));
      fillCircle(cv, cx + 4, cy - 12, 2.4f, C(80, 45, 25));
      for (int b = 0; b < bites; b++)  // bites eaten from the top
        fillCircle(cv, cx - 14 + b * 9, cy - r + 2, 9, C(0, 0, 0));
    }
    for (int i = 0; i < 6; i++) {  // crumbs falling
      float p = fmodf(et * 1.3f + i * 0.17f, 1.0f);
      fillCircle(cv, cx - 20 + i * 8 + sinf(i * 3.1f) * 4, cy - r + p * 60, 1.8f, C(210, 160, 90), 1 - p);
    }
  }
  if (emo == E_VOMIT) {
    const float heave = fmodf(et, 2.4f);
    const Col gA = C(140, 190, 50), gB = C(185, 200, 70), chunk = C(210, 175, 110), dark = C(90, 130, 30);
    const float mx = 120, my = 160;  // mouth
    const float S0 = 0.72f, S1 = 1.85f, FLOOR = 222, G = 520;
    // stream point launched at heave time e, seen s seconds later: shoots out forward-sideways,
    // swinging left/right as the head jerks, then falls with gravity (an arc, not a column)
    auto sp = [&](float e, float s, float &x, float &y) {
      float vx = 95 * sinf(e * 4.1f + 0.6f) + 20 * sinf(e * 13.7f);
      x = mx + vx * s;
      y = my + 10 + 20 * s + 0.5f * G * s * s;
    };
    // puddle: grows with every heave, with a wet highlight
    float pw = fminf(14 + et * 10, 82);
    for (int i = -3; i <= 3; i++)
      fillCircle(cv, mx + i * pw / 4.2f, FLOOR + 6 + fabsf(i) * 1.5f, pw / 3.4f, (i & 1) ? gA : dark, 0.92f);
    drawSegment(cv, mx - pw * 0.35f, FLOOR + 1, mx - pw * 0.05f, FLOOR, 2.2f, C(225, 240, 170), 0.6f);
    if (heave < S0) {
      // anticipation: cheeks bulge, swallow back once, then bulge bigger; lips pressed
      float p = heave / S0;
      float a = p < 0.4f ? sinf(p / 0.4f * kPI) * 0.6f : smoothstepf(0.45f, 1.0f, p);
      // puffed mouth: a green bulge with lips pressed tight (wavy line), trembling
      float w = 10 + 9 * a, j = 1.2f * sinf(t * 45);
      fillCircle(cv, mx + j, my, w, C(120, 190, 65), 0.9f);
      fillCircle(cv, mx - w * 0.35f + j, my - w * 0.35f, w * 0.25f, C(220, 245, 190), 0.7f);  // shine
      for (int i = 0; i < 4; i++) {
        float x0 = mx - w + i * w / 2, x1 = x0 + w / 2;
        drawSegment(cv, x0 + j, my + (i & 1 ? 1.5f : -1.5f), x1 + j, my + (i & 1 ? -1.5f : 1.5f), 2.4f, C(40, 70, 20), 1);
      }
    } else {
      // mouth: opens wide during the spew, closes after
      float mo = heave < S1 ? sat((heave - S0) * 10) : sat(1 - (heave - S1) * 4);
      fillCircle(cv, mx, my + 2, 13 * mo + 2, C(25, 15, 10));
      // one continuous stream (tapered ribbon along the arc) with bands that flow down it
      const int NS = 26;
      float px = 0, py = 0; bool have = false, hit = false; float hx = mx;
      for (int k = 0; k <= NS; k++) {
        float s = k * 0.022f, e = heave - s;
        if (e < S0 || e > S1) { have = false; continue; }
        float x, y; sp(e, s, x, y);
        if (y > FLOOR) { y = FLOOR; hit = true; hx = x; }
        if (have) {
          float th = 15 - k * 0.32f;
          float band = 0.5f + 0.5f * sinf(e * 38);
          drawSegment(cv, px, py, x, y, th, mixc(gA, gB, band), 1);
          drawSegment(cv, px - th * 0.22f, py, x - th * 0.22f, y, th * 0.22f, C(215, 235, 150), 0.55f);  // wet shine
        }
        px = x; py = y; have = true;
        if (y >= FLOOR) break;
      }
      // chunks carried in the stream
      for (int i = 0; i < 7; i++) {
        float s = fmodf(t * 1.1f + i * 0.143f, 1.0f) * 0.5f, e = heave - s;
        if (e < S0 || e > S1) continue;
        float x, y; sp(e, s, x, y);
        if (y > FLOOR) continue;
        fillCircle(cv, x + 3 * sinf(i * 2.1f), y, 3 + (i % 3), i % 2 ? chunk : C(230, 200, 120), 0.95f);
      }
      // splash where the stream hits the floor: droplets thrown up and out
      if (hit) {
        for (int i = 0; i < 10; i++) {
          float a = fmodf(t * 2.6f + i * 0.1f, 1.0f) * 0.38f;
          float ang = -kPI / 2 + (i - 4.5f) * 0.28f;
          float v = 70 + 35 * ((i * 7) % 5) / 5.0f;
          float x = hx + cosf(ang) * v * a, y = FLOOR + sinf(ang) * v * a + 0.5f * G * a * a;
          if (y <= FLOOR + 2) fillCircle(cv, x, y, 3.2f * (1 - a * 1.5f) + 0.8f, (i & 1) ? gB : gA, 0.9f);
        }
      }
    }
    if (heave > 1.9f) {  // drool string hanging from the mouth
      float d = (heave - 1.9f) / 0.5f;
      drawSegment(cv, mx + 2, my + 8, mx + 3, my + 10 + d * 34, 3, gA, 1 - d * 0.5f);
      fillCircle(cv, mx + 3, my + 12 + d * 34, 4, gA, 1 - d * 0.5f);
    }
    for (int i = 0; i < 3; i++) {  // sweat drops
      float p = fmodf(t * 0.7f + i / 3.0f, 1.0f);
      fillCircle(cv, 196 + i * 6, 70 + p * 40, 3.5f, C(140, 200, 255), sinf(p * kPI));
    }
  }
  if (love > 0.5f) {  // a few sparkles around
    for (int i = 0; i < 3; i++) {
      float p = fmodf(t * 0.6f + i * 0.37f, 1.0f);
      float ang = i * 2.1f + 0.7f;
      drawSparkle(cv, 120 + cosf(ang) * 100, 120 + sinf(ang) * 92, 7 * sinf(p * kPI), C(255, 220, 240), 1);
    }
  }
}
