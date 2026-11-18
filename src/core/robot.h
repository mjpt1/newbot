// Robot behaviour: boot animation, then a living face that reacts to inputs.
#pragma once
#include "boot.h"
#include "face.h"

struct Inputs {
  bool imuOk = false;
  float ax = 0, ay = 0, az = 1;  // accelerometer, in g
  float gyro = 0;                // rotation speed, degrees/second (magnitude)
  int button = 0;                // 0 none, 1 click, 2 long press, 3 double click
  float battV = 0;               // battery volts, 0 = unknown
  int setEmotion = -1;           // from the web page
  float setHue = -1;
  int setClock = -1;             // 0/1
};

struct Robot {
  BootAnim boot;
  Face face;
  bool booting = true;
  bool clockMode = false;
  bool lowBattery = false;
  float t = 0;

  void begin(bool playBoot);
  void update(float dt, const Inputs &in);
  void render(Canvas &cv) const;

 private:
  Emotion mood = E_NEUTRAL;
  float tempUntil = 0, manualUntil = 0, nextMood = 12;
  float stillTime = 0, shakeAcc = 0, lastJolt = -10, wakeRestore = 0;
  float lax = 0, lay = 0, laz = 1, az0 = 1;
  float g0x = 0, g0y = 0, g0z = 1, calT = 0;  // resting gravity = "level" pose
  bool lpInit = false, asleep = false;
  // inner state (idea from QBIT / Stack-chan): how it feels about you, and how hungry it is
  float affection = 0.3f;   // -1 annoyed .. 1 loves you
  float hunger = 0;         // 0 full .. 1 starving (fills in ~25 min)
  float roughT[3] = {-99, -99, -99};  // times of the last rough hits
  float lastDizzy = -99, lastVomit = -99;

  void setTemp(Emotion e, float dur);
  void setMood(Emotion e);
  void wake();
  void sleep();
};
