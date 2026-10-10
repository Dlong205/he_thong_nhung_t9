#pragma once
#include <stdint.h>

// Don vi vat ly sau quy doi
typedef struct {
  float ax, ay, az;   // g
  float gx, gy, gz;   // dps, da tru bias
  float mx, my, mz;   // Gauss (QMC5883P, da tru hard-iron). 0 nếu không có mag.
  float dt;           // s
  uint32_t ts_ms;
} SensorData_t;

typedef struct {
  float roll_acc, pitch_acc;
  float roll_gyro, pitch_gyro;
  float roll_cf, pitch_cf;
  float roll_kalman, pitch_kalman;
  float yaw_raw, yaw_tilt; // deg [-180,180]. 0 nếu không có mag.
} Attitude_t;

// Cau hinh he thong (khoa theo de K03)
#define SYS_FS_HZ 500
#define SYS_TS (1.0f / SYS_FS_HZ)
#define SYS_TELEMETRY_DIV 10   // 500 -> 50Hz
#define CF_ALPHA_DEFAULT 0.98f
#define ACC_LSB_DEFAULT 16384.0f
#define GYRO_LSB_DEFAULT 131.0f
