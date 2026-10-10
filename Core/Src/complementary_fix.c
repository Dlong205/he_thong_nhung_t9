// SV2 - Complementary fixed-point Q16.16 (so sánh float vs fixed, đề K03 SV2).
// Giữ float ở đầu vào (deg), chỉ lượng tử hóa angle/alpha để đo sai số lượng tử.
#include "complementary_fix.h"
int32_t f2q(float x) { return (int32_t)(x * 65536.0f); }
float q2f(int32_t q) { return ((float)q) / 65536.0f; }
void cffix_init(CFfix_t *f, float alpha) {
  f->roll = 0; f->pitch = 0; f->alpha = f2q(alpha);
}
void cffix_update(CFfix_t *f, float racc, float pacc, float gx, float gy, float dt) {
  // roll = alpha*(roll + gx*dt) + (1-alpha)*racc, tính bằng Q16 với tích 64-bit
  int64_t one = 65536LL;
  int64_t a = f->alpha;
  int64_t b = one - a;
  int64_t r = ((a * (int64_t)(f->roll + f2q(gx * dt))) >> 16) + ((b * f2q(racc)) >> 16);
  int64_t p = ((a * (int64_t)(f->pitch + f2q(gy * dt))) >> 16) + ((b * f2q(pacc)) >> 16);
  f->roll = (int32_t)r; f->pitch = (int32_t)p;
}
