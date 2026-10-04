#ifndef MPU6500_I2C_H
#define MPU6500_I2C_H
#include "stm32f10x.h"
#include <stdint.h>

/* STM32F103: HCLK=72 MHz, APB1=36 MHz. Owns I2C1, TIM2, EXTI0/PB0.
 * PB6=SCL, PB7=SDA; pull-ups to 3.3 V; MPU CS=3.3 V, AD0=GND.
 * This is an I2C implementation, not the SPI transport in the original plan.
 */
#define MPU6500_I2C_ADDR_AD0_LOW  0x68u
#define MPU6500_I2C_ADDR_AD0_HIGH 0x69u
#define MPU6500_I2C_ADDRESS (MPU6500_I2C_ADDR_AD0_LOW << 1)
#define ACCEL_SCALE_4G 8192.0f
#define GYRO_SCALE_500DPS 65.5f
#define MPU_SAMPLE_ERROR (-1)
#define MPU_SAMPLE_NONE 0
#define MPU_SAMPLE_OK 1

typedef struct {
    float acc_offset[3]; /* raw LSB */
    float acc_scale[3];  /* dimensionless correction, default 1 */
    float gyro_bias[3];  /* raw LSB */
} MPU_Calib_t;

typedef struct {
    float ax, ay, az; /* g, sensor axes */
    float gx, gy, gz; /* degrees/second, sensor axes */
    float dt;        /* seconds; first sample = 0, do not integrate */
    uint32_t timestamp; /* microseconds at serviced DRDY IRQ, wraps ~71.6 min */
} SensorData_t;

extern volatile uint8_t g_mpu_data_ready, g_mpu_id;
extern volatile uint32_t g_mpu_irq_count, g_mpu_irq_time_us, g_mpu_dropped;

/* Setup/init/calibration: 1=success, 0=failure. */
uint8_t MPU6500_I2C_LowLevel_Init(void);
uint8_t MPU6500_Init_I2C(void);
void delay_ms(volatile uint32_t ms); /* only after low-level init */
uint32_t MPU6500_TimeUs(void);
void MPU6500_OnDataReady(void);
void MPU6500_ResetAcquisition(void);

/* Bus/raw: 0=success, 1=failure. dev_addr is shifted 8-bit write address. */
uint8_t I2C_WriteReg(uint8_t dev_addr, uint8_t reg_addr, uint8_t data);
uint8_t I2C_ReadReg(uint8_t dev_addr, uint8_t reg_addr, uint8_t *value);
uint8_t I2C_ReadBurst(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint16_t len);
uint8_t MPU6500_ReadRaw_I2C(int16_t acc[3], int16_t gyro[3]);
void MPU6500_GetScaled_I2C(int16_t raw_acc[3], int16_t raw_gyro[3],
    const MPU_Calib_t *calib, float acc_g[3], float gyro_dps[3]);
void MPU6500_CalibDefault(MPU_Calib_t *calib);
uint8_t MPU6500_CalibGyro_I2C(MPU_Calib_t *calib, uint16_t samples);
uint8_t MPU6500_CalibAccel6Face_I2C(MPU_Calib_t *calib,
    float ax_max, float ax_min, float ay_max, float ay_min, float az_max, float az_min);
int MPU6500_ReadSample_I2C(const MPU_Calib_t *calib, SensorData_t *out);
uint8_t MPU6500_MeanAccelFace(uint16_t samples, float mean_raw[3]);
#endif
