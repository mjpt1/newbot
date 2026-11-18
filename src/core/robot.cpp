#if defined(__XTENSA__)
#pragma GCC optimize("fast-math")  // hardware float divide/sqrt instead of slow library calls
#endif
#include "robot.h"

// Flip these if the eyes slide the wrong way when you tilt the robot.
static const float TILT_X_SIGN = 1.0f, TILT_Y_SIGN = 1.0f;
static const bool TILT_SWAP_XY = false;

static const float SLEEPY_AFTER = 90, SLEEP_AFTER = 180;  // seconds without movement

void Robot::begin(bool playBoot) {
  boot.reset();
  booting = playBoot;
  if (!playBoot) face.setEmotion(E_NEUTRAL);
}

void Robot::setMood(Emotion e) {
  mood = e;
  if (t >= tempUntil) face.setEmotion(e);
}

void Robot::setTemp(Emotion e, float dur) {
  face.setEmotion(e);
  tempUntil = t + dur;
}

void Robot::sleep() {
  asleep = true;
  face.setEmotion(E_ASLEEP);
  tempUntil = 0;
}

void Robot::wake() {
  asleep = false;
  stillTime = 0;
  setTemp(E_SURPRISED, 1.1f);
  mood = lowBattery ? E_SLEEPY : E_NEUTRAL;
}

void Robot::update(float dt, const Inputs &in) {
  t += dt;
  if (in.setHue >= 0) { face.hue.set(in.setHue); face.tint.set(1); }
  else if (in.setHue < -1.5f) face.tint.set(0);  // -2 = back to white
  // affection slowly drifts back to "friendly", hunger slowly rises
  affection += (0.3f - affection) * (1 - expf(-dt / 120));
  if (!asleep) hunger = fminf(1, hunger + dt / 1500);
  if (in.setClock >= 0) clockMode = in.setClock;

  if (booting) {
    boot.update(dt);
    if (in.button) boot.t = bootT::END;  // any button press skips the intro
    if (boot.done()) {
      booting = false;
      face.startClosed();
      face.openL.response = face.openR.response = 0.45f;  // slow, sleepy first opening
      wakeRestore = t + 1.6f;
      face.setEmotion(E_NEUTRAL);
      face.lookAt(0, 0, 1.5f);
    }
    face.update(dt);
    return;
  }
  if (wakeRestore > 0 && t > wakeRestore) { face.openL.response = face.openR.response = 0.10f; wakeRestore = 0; }

  // ---- battery ----
  lowBattery = in.battV > 2.5f && in.battV < 3.45f;

  // ---- motion sensor ----
  if (in.imuOk) {
    float k = 1 - expf(-dt * 6);
    if (!lpInit) { lax = in.ax; lay = in.ay; laz = in.az; g0x = lax; g0y = lay; g0z = laz; lpInit = true; }
    // jolt = raw accel minus its smoothed value (gravity removed)
    float jx = in.ax - lax, jy = in.ay - lay, jz = in.az - laz;
    float jolt = sqrtf(jx * jx + jy * jy + jz * jz);
    lax += (in.ax - lax) * k; lay += (in.ay - lay) * k; laz += (in.az - laz) * k;
    bool moving = in.gyro > 12 || jolt > 0.06f;
    stillTime = moving ? 0 : stillTime + dt;

    // the pose it rests in counts as "level": learn it in the first 2 s, then drift slowly while still
    calT += dt;
    float ck = calT < 2 ? 1 - expf(-dt * 4) : (stillTime > 3 ? 1 - expf(-dt * 0.08f) : 0);
    g0x += (lax - g0x) * ck; g0y += (lay - g0y) * ck; g0z += (laz - g0z) * ck;

    float gl = sqrtf(g0x * g0x + g0y * g0y + g0z * g0z) + 1e-3f;
    float ll = sqrtf(lax * lax + lay * lay + laz * laz) + 1e-3f;
    bool upsideDown = (lax * g0x + lay * g0y + laz * g0z) / (gl * ll) < -0.5f;
    if (upsideDown && !asleep) sleep();
    else if (asleep && moving && !upsideDown && t > manualUntil) wake();

    if (!asleep) {
      // tilt relative to the resting pose: eyes slide toward the low side.
      // Measured on this board (held upright, USB at the bottom):
      //   gravity = -Y; tilt right -> gravity moves to -X; tilt top back -> gravity moves to -Z.
      float nx = lax / ll, nz = laz / ll;
      float n0x = g0x / gl, n0z = g0z / gl;
      float tx = -(nx - n0x), ty = (nz - n0z);
      if (TILT_SWAP_XY) { float tmp = tx; tx = ty; ty = tmp; }
      auto band = [](float v) { return v / (1 + fabsf(v) * 0.7f); };  // rubber-band near the limits
      face.biasX = band(tx * 2.6f * TILT_X_SIGN);   // ~25 deg of tilt = full look
      face.biasY = band(ty * 2.6f * TILT_Y_SIGN);
      // knock / jolt -> surprised
      // gentle handling (slow tilting, no hits) = being played with: it likes that
      if (moving && jolt < 0.25f && in.gyro < 80) affection = fminf(1, affection + dt * 0.04f);
      if (jolt > 0.6f && t - lastJolt > 2.0f) {
        lastJolt = t;
        affection = fmaxf(-1, affection - 0.25f);
        roughT[0] = roughT[1]; roughT[1] = roughT[2]; roughT[2] = t;
        if (t - roughT[0] < 12) { setTemp(E_SWEAR, 3.5f); roughT[2] = -99; }  // 3 hits in 12 s: swears
        else if (affection < -0.2f) setTemp(E_ANGRY, 2.5f);                 // keeps getting hit: angry
        else setTemp(E_SURPRISED, 1.3f);
      }
      // shaking -> dizzy; shaken again while still dizzy -> throws up
      shakeAcc += (in.gyro > 160 || jolt > 0.6f) ? dt : -dt * 0.7f;
      if (shakeAcc < 0) shakeAcc = 0;
      if (shakeAcc > 0.8f) {
        shakeAcc = 0; lastJolt = t;
        affection = fmaxf(-1, affection - 0.3f);
        // throws up only if shaken hard again soon after, and at most once a minute
        if (t - lastDizzy < 5 && t - lastVomit > 60) { setTemp(E_VOMIT, 4.0f); lastDizzy = -99; lastVomit = t; }
        else { setTemp(E_DIZZY, 3.5f); lastDizzy = t; }
      }
      // left alone -> sleepy -> asleep
      if (stillTime > SLEEP_AFTER) sleep();
      else if (stillTime > SLEEPY_AFTER && mood != E_SLEEPY && t > manualUntil) setMood(E_SLEEPY);
      else if (stillTime < 1 && mood == E_SLEEPY && !lowBattery) setMood(E_NEUTRAL);
    } else {
      face.biasX = face.biasY = 0;
    }
  }

  // ---- button ----
  if (in.button == 1) {  // click: next emotion
    int e = ((int)face.emo + 1) % E_COUNT;
    if (asleep) wake();
    else {
      manualUntil = t + 20; tempUntil = 0; setMood((Emotion)e);
      if (e == E_ASLEEP) asleep = true;
      face.scaleL.kick(2.2f); face.scaleR.kick(2.2f);  // visible "pop" so the click is obvious
      affection = fminf(1, affection + 0.08f);
    }
  } else if (in.button == 2) {  // long press: sleep / wake
    if (asleep) wake(); else { sleep(); manualUntil = t + 5; }
  } else if (in.button == 3) {  // double click: clock
    clockMode = !clockMode;
  }

  // ---- web ----
  if (in.setEmotion >= 0 && in.setEmotion < E_COUNT) {
    manualUntil = t + 30; tempUntil = 0;
    if (in.setEmotion == E_ASLEEP) sleep();
    else { asleep = false; stillTime = 0; setMood((Emotion)in.setEmotion); }
    if (in.setEmotion == E_EAT) { hunger = 0; affection = fminf(1, affection + 0.3f); manualUntil = t + 6; }  // fed!
  }

  // ---- temporary emotion ended ----
  if (tempUntil > 0 && t >= tempUntil) {
    tempUntil = 0;
    if (!asleep) face.setEmotion(mood);
  }

  // ---- own moods when nobody is playing with it ----
  if (!asleep && t > manualUntil && tempUntil == 0 && t > nextMood) {
    nextMood = t + rng.f(10, 24);
    if (lowBattery) setMood(E_SLEEPY);
    else if (hunger > 0.8f) {  // starving: sad, and now and then dreams of food
      if (rng.i(3) == 0) setTemp(E_EAT, 3.0f); else setMood(E_SAD);
    } else if (affection < -0.3f) {  // still upset with you
      setMood(rng.i(3) == 0 ? E_ANGRY : E_SAD);
    } else if (mood != E_SLEEPY) {
      // the more it likes you, the happier its idle moods
      int r = rng.i(100) + (int)(affection * 25);
      if (r < 35) setMood(E_NEUTRAL);
      else if (r < 55) setMood(E_CURIOUS);
      else if (r < 80) setMood(E_HAPPY);
      else if (r < 95) setTemp(E_WINK, 0.7f);
      else setTemp(E_LOVE, 2.5f);
    }
  }

  // ---- clock layout: smaller eyes up top ----
  face.faceScale.set(clockMode ? 0.62f : 1.0f);
  face.faceY.set(clockMode ? -38.0f : 0.0f);
  face.update(dt);
}

static void batteryIcon(Canvas &cv) {
  Col red = C(255, 70, 60);
  float x = 120, y = 214;
  drawSegment(cv, x - 12, y - 6, x + 10, y - 6, 2, red);
  drawSegment(cv, x - 12, y + 6, x + 10, y + 6, 2, red);
  drawSegment(cv, x - 12, y - 6, x - 12, y + 6, 2, red);
  drawSegment(cv, x + 10, y - 6, x + 10, y + 6, 2, red);
  drawSegment(cv, x + 13, y - 2, x + 13, y + 2, 3, red);
  drawSegment(cv, x - 8, y - 2, x - 8, y + 2, 3, red);
}

void Robot::render(Canvas &cv) const {
  if (booting) { boot.render(cv); return; }
  cv.clearBlack();
  face.render(cv);
  if (lowBattery && fmodf(t, 2.0f) < 1.4f) batteryIcon(cv);
}
