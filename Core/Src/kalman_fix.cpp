#include "kalman_fix.h"
// angle/bias Q16.16 (nhan chia int64), P/Q/R float vi qua nho voi Q16.
static int32_t f_to_q(float x) { return (int32_t)(x * 65536.0f); }
static float q_to_f(int32_t q) { return q / 65536.0f; }
void kfix_init(KalmanFix_t *k) {
  k->angle = 0; k->bias = 0;
  k->P00 = 0; k->P01 = 0; k->P10 = 0; k->P11 = 0;
  k->Q_angle = 0.001f; k->Q_bias = 0.003f; k->R_measure = 0.03f;
}
float kfix_update(KalmanFix_t *k, float acc_angle, float gyro_rate, float dt) {
  float rate = q_to_f(k->bias);
  rate = gyro_rate - rate;
  int32_t qrate = f_to_q(rate), qdt = f_to_q(dt);
  k->angle += (int32_t)(((int64_t)qdt * qrate) >> 16);
  k->P00 += dt * (dt * k->P11 - k->P01 - k->P10 + k->Q_angle);
  k->P01 -= dt * k->P11; k->P10 -= dt * k->P11; k->P11 += k->Q_bias * dt;
  float y = acc_angle - q_to_f(k->angle);
  float S = k->P00 + k->R_measure;
  float K0 = k->P00 / S, K1 = k->P10 / S;
  k->angle += f_to_q(K0 * y); k->bias += f_to_q(K1 * y);
  float P00t = k->P00, P01t = k->P01;
  k->P00 -= K0 * P00t; k->P01 -= K0 * P01t; k->P10 -= K1 * P00t; k->P11 -= K1 * P01t;
  return q_to_f(k->angle);
}
