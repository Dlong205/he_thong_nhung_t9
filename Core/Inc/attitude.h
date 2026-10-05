#pragma once
#include "types.h"
// Baseline: goc acc + tich phan gyro
void attitude_acc(const SensorData_t *s, float *roll, float *pitch);
typedef struct { float roll, pitch; } GyroInt_t;
void gyro_int_reset(GyroInt_t *g);
void gyro_int_update(GyroInt_t *g, const SensorData_t *s);
