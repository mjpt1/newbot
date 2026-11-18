// Robot face: two shaded "glass" eyes with eyelids, driven by springs.
#pragma once
#include "gfx.h"
#include "spring.h"

enum Emotion {
  E_NEUTRAL, E_HAPPY, E_SAD, E_ANGRY, E_SURPRISED, E_SLEEPY,
  E_DIZZY, E_LOVE, E_CURIOUS, E_WINK, E_ASLEEP,
  E_SWEAR, E_EAT, E_VOMIT, E_COUNT
};
extern const char *const kEmotionName[E_COUNT];

struct Face {
  // look
  Spring hue{160, 0.6f, 1.0f};            // eye colour (mint = 160)
  Spring tint{0, 0.6f, 1.0f};             // 0 = white eyes, 1 = full colour
  Spring gx{0, 0.12f, 0.84f}, gy{0, 0.12f, 0.84f};  // gaze, -1..1 (slight overshoot = snappy saccade)
  Spring openL{1, 0.10f, 1}, openR{1, 0.10f, 1};
  Spring tilt{0, 0.22f, 1};               // >0 angry (inner lid down), <0 sad
  Spring happy{0, 0.20f, 0.85f};          // lower lid pushes up (smiling cheeks)
  Spring scaleL{1, 0.28f, 0.65f}, scaleR{1, 0.28f, 0.65f};
  Spring pupil{5, 0.18f, 0.8f};
  Spring yoff{0, 0.35f, 1};
  Spring love{0, 0.3f, 1}, dizzy{0, 0.4f, 1};
  Spring rage{0, 0.35f, 1};               // 1 = fiery red eyes (angry / swearing)
  Spring faceScale{1, 0.45f, 1}, faceY{0, 0.45f, 1};  // used by clock mode

  Emotion emo = E_NEUTRAL;
  float t = 0;
  float emoT = 0;  // time when the current emotion started (drives eat/vomit/swear loops)
  float biasX = 0, biasY = 0;   // from tilting the robot (IMU)
  bool autoLook = true;

  void setEmotion(Emotion e);
  void blink();
  void lookAt(float x, float y, float hold);
  void startClosed();  // begin fully shut (for the wake-up)
  void update(float dt);
  void render(Canvas &cv) const;

 private:
  float blinkT = -10, nextBlink = 2.5f, nextLook = 1, nextMicro = 0.4f;
  float lookX = 0, lookY = 0, microX = 0, microY = 0, holdUntil = 0;
  float blinkMul() const;
  void renderEye(Canvas &cv, int side) const;
};
