// Power-on animation rebuilt from the reference videos.
#pragma once
#include "gfx.h"

namespace bootT {
static const float FADE = 1.2f;     // black -> purple -> pink -> white
static const float MICROBE = 10.4f; // dot roams on white
static const float BLACK = 11.3f;
static const float GROW = 19.3f;    // dot grows into a wobbly blob
static const float SPLIT = 22.7f;   // blob divides in two
static const float CLOSED = 23.7f;  // thin closed-eye arcs
static const float SLOT = 33.5f;    // flips through eye styles
static const float END = 36.0f;     // sleeping mint eyes, then the face takes over
}

struct BootAnim {
  float t = 0;
  void reset();
  void update(float dt);
  void render(Canvas &cv) const;
  bool done() const { return t >= bootT::END; }

 private:
  // microbe
  float mx, my, mvx, mvy, curl, squash, squashAng, nextDart;
  // blob radius tables (per angle)
  static const int NA = 128;
  float rOut[NA], rIn[NA];
  // slot machine
  int cur, prev, slideDir;
  float changedAt, nextAt;
  struct Spark { float x, y, born, life; } sparks[6];

  void updateMicrobe(float dt);
  void renderMicrobe(Canvas &cv) const;
  void renderGrow(Canvas &cv) const;
  void renderSplit(Canvas &cv) const;
  void renderSlot(Canvas &cv) const;
  void renderStyle(Canvas &cv, int s, float ox) const;
};
