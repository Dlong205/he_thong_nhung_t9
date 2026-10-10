#include "yaw.h"
#include <math.h>
#define RAD2DEG 57.29578f
#define DEG2RAD 0.01745329f

float yaw_raw(float mx, float my) {
  return atan2f(-my, mx) * RAD2DEG;
}

float yaw_tilt(float mx, float my, float mz, float roll_deg, float pitch_deg) {
  float r = roll_deg * DEG2RAD;
  float p = pitch_deg * DEG2RAD;
  float cr = cosf(r), sr = sinf(r), cp = cosf(p), sp = sinf(p);
  float xp = mx * cp + mz * sp;
  float yp = mx * sr * sp + my * cr - mz * sr * cp;
  return atan2f(-yp, xp) * RAD2DEG;
}

float yaw_wrap180(float yaw) {
  while (yaw > 180.0f) yaw -= 360.0f;
  while (yaw <= -180.0f) yaw += 360.0f;
  return yaw;
}
