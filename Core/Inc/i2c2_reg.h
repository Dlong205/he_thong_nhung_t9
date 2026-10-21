#pragma once
// SV1 - I2C2 PB10(SCL)/PB11(SDA) 100kHz Standard-mode, MỨC THANH GHI THẬT (RM0008 §26).
// Đây là phần chấm điểm Khó: không dùng Wire/HAL_I2C cho đường đọc MPU.
//   RCC_APB1ENR.I2C2EN + RCC_APB2ENR.IOPBEN/AFIOEN
//   GPIOB_CRH: PB10/PB11 Alternate Open-Drain 50MHz (CNF=11 MODE=11)
//   I2C_CR2.FREQ = PCLK1(MHz)=36; I2C_CCR Standard: CCR=PCLK1/(2*100kHz)=180
//   I2C_TRISE Standard: FREQ+1 = 37
//   Luồng: SB -> ADDR -> TXE/BTF -> DR ... polling SR1/SR2, timeout chống treo bus.
#include <stdint.h>
#include <stdbool.h>
void i2c2_init_100k(void);
bool i2c2_write_reg(uint8_t dev7, uint8_t reg, uint8_t val);
bool i2c2_read_reg(uint8_t dev7, uint8_t reg, uint8_t *val);
bool i2c2_burst_read(uint8_t dev7, uint8_t start_reg, uint8_t *buf, uint16_t len);
bool i2c2_write_buf(uint8_t dev7, const uint8_t *buf, uint16_t len); // ghi thô N byte (OLED cmd/data stream)
bool i2c2_probe(uint8_t dev7); // phát START+ADDR, true nếu có ACK (quét bus bring-up)
// Tự cứu bus khi slave giữ SDA (glitch dây breadboard): tắt PE, bit-bang ≥9 xung
// SCL + STOP thủ công, rồi init lại. Gọi khi lỗi dồn (mpu_err), không gọi mỗi mẫu.
void i2c2_recover(void);
// Mã bước I2C lỗi gần nhất (0 = chưa lỗi): 11-15 write_reg, 21-26 read_reg,
// 31-42 burst_read. Đọc qua ST-Link khi bring-up, không cần UART.
extern volatile uint8_t i2c2_fail_at;
extern volatile uint16_t i2c2_fail_idx; // chỉ số byte đang dở khi burst fail
extern volatile uint32_t i2c2_sr1_last; // SR1 tại lúc fail
extern volatile uint32_t i2c2_sr2_last; // SR2 tại lúc fail
void i2c2_dump_regs(void); // CR1/CR2/CCR/TRISE ra USART làm bằng chứng
