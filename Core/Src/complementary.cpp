#include "complementary.h"
void cf_init(CF_t *f, float alpha) { f->roll = 0; f->pitch = 0; f->alpha = alpha; }
void cf_update(CF_t *f, float racc, float pacc, float gx, float gy, float dt) {
  f->roll = f->alpha * (f->roll + gx * dt) + (1 - f->alpha) * racc;
  f->pitch = f->alpha * (f->pitch + gy * dt) + (1 - f->alpha) * pacc;
}
