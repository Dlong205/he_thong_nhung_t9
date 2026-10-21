#pragma once
#include <stdint.h>
// Kalman hai trạng thái: angle Q16.16, bias Q10.22, P/Q/R Q4.28.
// float chỉ dùng ở ranh giới nhận mẫu và trả góc cho telemetry.
typedef struct {
  int32_t angle, bias;
  int32_t P00, P01, P10, P11;
  int32_t Q_angle, Q_bias, R_measure;
} KalmanFix_t;
void kfix_init(KalmanFix_t *k);
float kfix_update(KalmanFix_t *k, float acc_angle, float gyro_rate, float dt);
