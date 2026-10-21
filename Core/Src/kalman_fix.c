// SV2 - Kalman fixed-point hai trạng thái [angle; bias].
#include "kalman_fix.h"
#include <limits.h>

#define Q16_ONE 65536LL
#define Q22_ONE 4194304LL
#define Q24_ONE 16777216LL
// round(Q/R * 2^28), cùng tham số với kalman.c. LSB ~= 3.73e-9.
#define Q_ANGLE_Q28 268435
#define Q_BIAS_Q28 805306
#define R_MEASURE_Q28 8053064

static int32_t to_q16(float x) {
  if (x >= 32767.0f) return INT32_MAX;
  if (x <= -32768.0f) return INT32_MIN;
  return (int32_t)(x * (float)Q16_ONE);
}

static int32_t to_q22(float x) {
  if (x >= 511.0f) return INT32_MAX;
  if (x <= -512.0f) return INT32_MIN;
  return (int32_t)(x * (float)Q22_ONE);
}

static int32_t to_q24(float x) {
  if (x >= 127.0f) return INT32_MAX;
  if (x <= -128.0f) return INT32_MIN;
  return (int32_t)(x * (float)Q24_ONE);
}

static int32_t mul_q28_q16(int32_t a, int32_t b) {
  return (int32_t)(((int64_t)a * b) / Q16_ONE);
}

static int32_t mul_q28_dt(int32_t a, int32_t b) {
  return (int32_t)(((int64_t)a * b) / Q24_ONE);
}

static int32_t gain_q16(int32_t p_q28, int32_t s_q28) {
  // Chia 32-bit trực tiếp trên Cortex-M3. Chia cả tử/mẫu theo cùng tỉ lệ
  // để không cần phép chia 64-bit trong dải covariance thường gặp.
  if (p_q28 >= -(INT32_MAX / 256) && p_q28 <= INT32_MAX / 256 &&
      s_q28 >= 256) {
    return (p_q28 * 256) / (s_q28 / 256);
  }
  if (p_q28 >= -(INT32_MAX / 16) && p_q28 <= INT32_MAX / 16 &&
      s_q28 >= 4096) {
    return (p_q28 * 16) / (s_q28 / 4096);
  }
  return (int32_t)(((int64_t)p_q28 * Q16_ONE) / s_q28);
}

void kfix_init(KalmanFix_t *k) {
  k->angle = 0; k->bias = 0;
  k->P00 = 0; k->P01 = 0; k->P10 = 0; k->P11 = 0;
  k->Q_angle = Q_ANGLE_Q28;
  k->Q_bias = Q_BIAS_Q28;
  k->R_measure = R_MEASURE_Q28;
}
float kfix_update(KalmanFix_t *k, float acc_angle, float gyro_rate, float dt) {
  // Chuyển đổi đơn vị duy nhất tại biên API. Caller giới hạn dt <= 20 ms.
  int32_t dt_q24 = to_q24(dt);
  int32_t acc_q16 = to_q16(acc_angle);
  int32_t gyro_q22 = to_q22(gyro_rate);
  int64_t rate_q22 = (int64_t)gyro_q22 - k->bias;
  k->angle += (int32_t)((rate_q22 * dt_q24) / (1LL << 30)); // Q22*Q24 -> Q16

  int32_t p11 = k->P11;
  k->P00 += mul_q28_dt(mul_q28_dt(p11, dt_q24), dt_q24)
          - mul_q28_dt(k->P01 + k->P10, dt_q24)
          + mul_q28_dt(k->Q_angle, dt_q24);
  k->P01 -= mul_q28_dt(p11, dt_q24);
  k->P10 -= mul_q28_dt(p11, dt_q24);
  k->P11 += mul_q28_dt(k->Q_bias, dt_q24);

  int64_t residual_q16 = (int64_t)acc_q16 - k->angle;
  int32_t innovation_q28 = k->P00 + k->R_measure;
  if (innovation_q28 <= 0) return (float)k->angle / (float)Q16_ONE;
  int32_t k0_q16 = gain_q16(k->P00, innovation_q28);
  int32_t k1_q16 = gain_q16(k->P10, innovation_q28);
  k->angle += (int32_t)((k0_q16 * residual_q16) / Q16_ONE);
  k->bias += (int32_t)((k1_q16 * residual_q16) / 1024LL); // Q32 -> Q22

  // Dùng covariance ngay trước bước đo, giống bản float.
  int32_t p00 = k->P00, p01 = k->P01;
  k->P00 -= mul_q28_q16(p00, k0_q16);
  k->P01 -= mul_q28_q16(p01, k0_q16);
  k->P10 -= mul_q28_q16(p00, k1_q16);
  k->P11 -= mul_q28_q16(p01, k1_q16);
  return (float)k->angle / (float)Q16_ONE;
}
