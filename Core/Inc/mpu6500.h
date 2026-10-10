#pragma once
#include <stdint.h>
#include <stdbool.h>

// MPU6500 I2C addr 7-bit 0x68 (AD0=GND). WHO_AM_I phai = 0x70.
// Driver dùng i2c_reg (thanh ghi thật), không dùng Wire/HAL_I2C.
bool mpu6500_init(void);   // wake + DLPF + scale + rate (~500Hz), tự dò addr 0x68/0x69
bool mpu6500_whoami(uint8_t *id);
uint8_t mpu6500_addr(void); // địa chỉ đã dò được (0 nếu chưa init)
void mpu6500_enable_data_ready(void); // INT_ENABLE.DATA_RDY_EN=1 (INT PA0, xung 50us)
bool mpu6500_read_raw(int16_t *ax, int16_t *ay, int16_t *az,
                      int16_t *gx, int16_t *gy, int16_t *gz);
void mpu6500_get_scale(float *acc_lsb, float *gyro_lsb);
