// Spring animation (Apple "Designing Fluid Interfaces" style):
// damping 1.0 = smooth, no overshoot; < 1.0 = a little bounce.
// response = roughly how many seconds it takes to get there.
// Always continues from the current value + velocity, so any change of
// target mid-motion stays smooth (interruptible).
#pragma once
#include <math.h>

struct Spring {
  float x = 0, v = 0, target = 0;
  float response = 0.35f, damping = 1.0f;

  Spring() {}
  Spring(float val, float resp, float damp) : x(val), target(val), response(resp), damping(damp) {}

  void set(float t) { target = t; }
  void snap(float val) { x = target = val; v = 0; }
  void kick(float vel) { v += vel; }

  void step(float dt) {
    if (dt <= 0) return;
    float w = 2.0f * 3.14159265f / response;
    float k = w * w, c = 2.0f * damping * w;
    int n = (int)ceilf(dt / 0.004f);
    float h = dt / n;
    for (int i = 0; i < n; i++) {  // semi-implicit Euler, small sub-steps = stable
      v += (-(x - target) * k - c * v) * h;
      x += v * h;
    }
  }
  operator float() const { return x; }
};
