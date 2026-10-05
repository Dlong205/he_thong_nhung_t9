#pragma once
// Kalman 2-state [angle; bias], float, tu viet
typedef struct {
  float angle, bias, P00, P01, P10, P11, Q_angle, Q_bias, R_measure;
} Kalman_t;
void kalman_init(Kalman_t *k);
float kalman_update(Kalman_t *k, float acc_angle, float gyro_rate, float dt);
