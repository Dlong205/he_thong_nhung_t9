#pragma once
// Mở rộng K03 - Từ kế QMC5883P (Yaw). Dùng i2c_reg chung bus với MPU+OLED.
// Địa chỉ TỰ DÒ trong {0x2C (chuẩn P), 0x0D (L/clone), 0x0C}: đọc CHIPID 0x00
// phải = 0x80. Nhiều module clone địa chỉ khác nhau nên không chốt cứng 0x2C.
// Map theo datasheet QST Rev.E + Adafruit lib:
//   CHIPID 0x00 (expect 0x80), X 0x01/02, Y 0x03/04, Z 0x05/06 (LE),
//   STATUS 0x09 (DRDY bit0), CTRL1 0x0A (MODE/ODR/OSR/DSR), CTRL2 0x0B (RANGE/SETRESET).
// Init: 0x06->0x29, 0x08->0x0B, 0xCD->0x0A (continuous 200Hz). Không chặn loop:
// đọc ở 50Hz (decimate trong main), poll DRDY có timeout.
#include <stdint.h>
#include <stdbool.h>
bool qmc_init(void);            // false nếu không thấy (vẫn chạy tiếp không Yaw)
bool qmc_present(void);
uint8_t qmc_addr(void);         // địa chỉ đã dò được (0 nếu absent)
bool qmc_data_ready(void);
bool qmc_read_raw(int16_t *mx, int16_t *my, int16_t *mz); // LSB, LE
void qmc_get_gauss_scale(float *lsb_per_gauss); // theo RANGE hiện tại
// Calib hard-iron: xoay ngang 360° rồi gọi collect mỗi mẫu, xong gọi finish.
void qmc_calib_reset(void);
void qmc_calib_collect(int16_t mx, int16_t my, int16_t mz);
bool qmc_calib_finish(void);    // false nếu chưa đủ biên độ xoay
void qmc_calib_get(float *ox, float *oy, float *oz);
