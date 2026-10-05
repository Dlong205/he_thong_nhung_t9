#pragma once
typedef struct { float roll, pitch, alpha; } CF_t;
void cf_init(CF_t *f, float alpha);
void cf_update(CF_t *f, float racc, float pacc, float gx, float gy, float dt);
