// SV2 - Kalman fixed-point: angle/bias Q16.16, P/Q/R float.
// Lý do kiến trúc (đã đo max|e|=0.01°/330s): P/Q/R ~1e-3..1e-6 < LSB Q16 (1.5e-5)
// nên giữ float cho covariance, chỉ lượng tử hóa state.
#include "kalman_fix.h"
void kfix_init(KalmanFix_t *k) {
  k->angle = 0; k->bias = 0;
  k->P00 = 0; k->P01 = 0; k->P10 = 0; k->P11 = 0;
  k->Q_angle = 0.001f; k->Q_bias = 0.003f; k->R_measure = 0.03f;
}
float kfix_update(KalmanFix_t *k, float acc_angle, float gyro_rate, float dt) {
  float angle = ((float)k->angle) / 65536.0f;
  float bias = ((float)k->bias) / 65536.0f;
  float rate = gyro_rate - bias;
  angle += dt * rate;
  k->P00 += dt * (dt * k->P11 - k->P01 - k->P10 + k->Q_angle);
  k->P01 -= dt * k->P11; k->P10 -= dt * k->P11; k->P11 += k->Q_bias * dt;
  float y = acc_angle - angle;
  float S = k->P00 + k->R_measure;
  float K0 = k->P00 / S, K1 = k->P10 / S;
  angle += K0 * y; bias += K1 * y;
  float P00t = k->P00, P01t = k->P01;
  k->P00 -= K0 * P00t; k->P01 -= K0 * P01t; k->P10 -= K1 * P00t; k->P11 -= K1 * P01t;
  k->angle = (int32_t)(angle * 65536.0f);
  k->bias = (int32_t)(bias * 65536.0f);
  return angle;
}
