#include "kalman.h"
void kalman_init(Kalman_t *k) {
  k->angle = 0; k->bias = 0; k->P00 = 0; k->P01 = 0; k->P10 = 0; k->P11 = 0;
  k->Q_angle = 0.001f; k->Q_bias = 0.003f; k->R_measure = 0.03f;
}
float kalman_update(Kalman_t *k, float acc_angle, float gyro_rate, float dt) {
  float rate = gyro_rate - k->bias;
  k->angle += dt * rate;
  k->P00 += dt * (dt * k->P11 - k->P01 - k->P10 + k->Q_angle);
  k->P01 -= dt * k->P11; k->P10 -= dt * k->P11; k->P11 += k->Q_bias * dt;
  float y = acc_angle - k->angle;
  float S = k->P00 + k->R_measure;
  float K0 = k->P00 / S, K1 = k->P10 / S;
  k->angle += K0 * y; k->bias += K1 * y;
  float P00t = k->P00, P01t = k->P01;
  k->P00 -= K0 * P00t; k->P01 -= K0 * P01t; k->P10 -= K1 * P00t; k->P11 -= K1 * P01t;
  return k->angle;
}
