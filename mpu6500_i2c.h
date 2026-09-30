#ifndef MPU6500_I2C_H
#define MPU6500_I2C_H

#include "stm32f10x.h"
#include <stdint.h>
#include <stdbool.h>

/* ====================================================================
 * SƠ ĐỒ KẾT NỐI I2C (STM32F103C8T6 "Blue Pill" - I2C1)
 * ====================================================================
 * PB6: SCL (Alternate Function Open-Drain)
 * PB7: SDA (Alternate Function Open-Drain)
 * PB0: INT (Ngắt EXTI0 - Data Ready)
 * AD0: Nối GND (Địa chỉ I2C = 0x68) hoặc nối 3.3V (Địa chỉ 0x69)
 *
 * MPU6500 Address: 0x68 (Khi AD0 = 0)
 * ==================================================================== */

#define MPU6500_I2C_ADDR_AD0_LOW  0x68
#define MPU6500_I2C_ADDR_AD0_HIGH 0x69

/* Mặc định sử dụng địa chỉ 0x68 (dịch trái 1 bit cho STM32 là 0xD0) */
#define MPU6500_I2C_ADDRESS (MPU6500_I2C_ADDR_AD0_LOW << 1) 

/* Độ nhạy phần cứng: Accel ±4g (8192 LSB/g), Gyro ±500 dps (65.5 LSB/dps) */
#define ACCEL_SCALE_4G      8192.0f
#define GYRO_SCALE_500DPS   65.5f

extern volatile uint8_t g_mpu_data_ready;
extern volatile uint8_t g_mpu_id;

void delay_ms(volatile uint32_t ms);

typedef struct {
    float acc_offset[3]; 
    float acc_scale[3];  
    float gyro_bias[3];  
} MPU_Calib_t;

/* Khởi tạo ngoại vi STM32F103 cho I2C (I2C1 trên PB6, PB7 và EXTI0 trên PB0) */
void MPU6500_I2C_LowLevel_Init(void);

/* Ghi/Đọc thanh ghi qua I2C (trả về 0=OK, 1=lỗi timeout) */
uint8_t I2C_WriteReg(uint8_t dev_addr, uint8_t reg_addr, uint8_t data);
uint8_t I2C_ReadReg(uint8_t dev_addr, uint8_t reg_addr);
uint8_t I2C_ReadBurst(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint16_t len);

/* Khởi tạo cảm biến MPU6500 qua I2C */
uint8_t MPU6500_Init_I2C(void);

/* Đọc dữ liệu raw 14 bytes (trả về 0=OK, 1=lỗi I2C - bỏ mẫu khi lỗi) */
uint8_t MPU6500_ReadRaw_I2C(int16_t acc[3], int16_t gyro[3]);

/* Áp dụng hiệu chuẩn ra đơn vị vật lý */
void MPU6500_GetScaled_I2C(int16_t raw_acc[3], int16_t raw_gyro[3],
                           const MPU_Calib_t *calib,
                           float acc_g[3], float gyro_dps[3]);

/* Tự động lấy Bias Gyro khi để yên cảm biến */
void MPU6500_CalibGyro_I2C(MPU_Calib_t *calib, uint16_t samples);

/* Tính Offset và Scale từ giá trị Min/Max của 6 mặt */
void MPU6500_CalibAccel6Face_I2C(MPU_Calib_t *calib, 
                                 float ax_max, float ax_min,
                                 float ay_max, float ay_min,
                                 float az_max, float az_min);

#endif /* MPU6500_I2C_H */
