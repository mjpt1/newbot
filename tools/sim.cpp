// PC preview: runs the same robot code and saves frames as PPM images.
// usage: sim <outdir> <t1> <t2> ...   (times in seconds since power-on)
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include "../src/core/robot.h"

int main(int argc, char **argv) {
  if (argc < 3) return 1;
  std::vector<float> times;
  for (int i = 2; i < argc; i++) times.push_back((float)atof(argv[i]));
  static uint16_t buf[240 * 240];
  Canvas cv; cv.buf = buf;
  Robot robot; robot.begin(true);
  const float dt = 1.0f / 40;
  float t = 0; size_t next = 0;
  // demo script after boot: show each emotion for 2.5 s
  float demoStart = 40;
  while (next < times.size()) {
    Inputs in;
    if (t >= demoStart) {
      int k = (int)((t - demoStart) / 2.5f);
      static int last = -1;
      if (k != last && k < E_COUNT) { in.setEmotion = k; last = k; }
    }
    robot.update(dt, in);
    t += dt;
    if (t >= times[next]) {
      robot.render(cv);
      char name[512]; snprintf(name, sizeof name, "%s/f_%07.2f.ppm", argv[1], times[next]);
      FILE *f = fopen(name, "wb");
      fprintf(f, "P6 240 240 255\n");
      for (int i = 0; i < 240 * 240; i++) {
        uint16_t s = buf[i], v = (uint16_t)((s >> 8) | (s << 8));
        unsigned char px[3] = {(unsigned char)(((v >> 11) & 31) * 255 / 31), (unsigned char)(((v >> 5) & 63) * 255 / 63),
                               (unsigned char)((v & 31) * 255 / 31)};
        fwrite(px, 1, 3, f);
      }
      fclose(f);
      next++;
    }
  }
  return 0;
}
