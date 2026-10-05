#pragma once
#include <stdint.h>
// Kalman fixed-point: angle/bias Q16.16, P/Q/R float.
// Ly do: P/Q/R ~1e-3..1e-6, Q16.16 (LSB 1.5e-5) khong du phan giai cho covariance -> giu float cho P.
typedef struct { int32_t angle, bias; float P00, P01, P10, P11, Q_angle, Q_bias, R_measure; } KalmanFix_t;
void kfix_init(KalmanFix_t *k);
float kfix_update(KalmanFix_t *k, float acc_angle, float gyro_rate, float dt);
