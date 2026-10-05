#pragma once
#include <stdint.h>
// Fixed-point Q16.16 cho so sanh float vs fixed (de K03 SV2)
// angle/rate don vi deg, alpha Q16
typedef struct { int32_t roll, pitch; int32_t alpha; } CFfix_t;
int32_t f2q(float x);
float q2f(int32_t q);
void cffix_init(CFfix_t *f, float alpha);
void cffix_update(CFfix_t *f, float racc, float pacc, float gx, float gy, float dt);
