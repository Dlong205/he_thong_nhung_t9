#include "complementary_fix.h"
int32_t f2q(float x) { return (int32_t)(x * 65536.0f); }
float q2f(int32_t q) { return q / 65536.0f; }
void cffix_init(CFfix_t *f, float alpha) { f->roll = 0; f->pitch = 0; f->alpha = f2q(alpha); }
void cffix_update(CFfix_t *f, float racc, float pacc, float gx, float gy, float dt) {
  // roll = alpha*(roll + gx*dt) + (1-alpha)*racc, giu nguyen float cho gx*dt/racc roi doi sang Q de tranh overflow
  float nr = q2f(f->alpha) * (q2f(f->roll) + gx * dt) + (1 - q2f(f->alpha)) * racc;
  float np = q2f(f->alpha) * (q2f(f->pitch) + gy * dt) + (1 - q2f(f->alpha)) * pacc;
  f->roll = f2q(nr); f->pitch = f2q(np);
}
