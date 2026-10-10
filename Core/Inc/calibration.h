#pragma once
// SV1 - Gyro bias (dps) + Acc 6-mặt offset/scale.
// Calib gyro giữ yên ~2s ở boot (được delay ở boot, cấm delay trong loop chính).
typedef struct { float gx, gy, gz; } GyroBias_t;
typedef struct { float ox, oy, oz, sx, sy, sz; } AccCal_t;
GyroBias_t calibration_gyro_bias(void);
AccCal_t calibration_acc_default(void);
void acc_apply(const AccCal_t *c, float *axg, float *ayg, float *azg);
