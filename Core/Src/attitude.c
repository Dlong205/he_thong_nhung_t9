// SV2 - Baseline acc + gyro int, tự viết (không DMP/lib).
#include "attitude.h"
#include <math.h>
#define RAD2DEG 57.29578f
void attitude_acc(const SensorData_t *s, float *roll, float *pitch) {
  *roll = atan2f(s->ay, s->az) * RAD2DEG;
  *pitch = atan2f(-s->ax, sqrtf(s->ay * s->ay + s->az * s->az)) * RAD2DEG;
}
void gyro_int_reset(GyroInt_t *g) { g->roll = 0; g->pitch = 0; }
void gyro_int_update(GyroInt_t *g, const SensorData_t *s) {
  g->roll += s->gx * s->dt; g->pitch += s->gy * s->dt;
}
