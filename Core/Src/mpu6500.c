// SV2 - MPU6500 driver trên i2c_reg. Register map theo datasheet PS:
// PWR_MGMT_1 0x6B=0x01 (PLL gyroX), SMPLRT_DIV 0x19=0x01 (500Hz khi DLPF bật),
// CONFIG 0x1A=0x03 (gyro DLPF 42Hz), GYRO_CONFIG 0x1B=0x00 (+-250dps),
// ACCEL_CONFIG 0x1C=0x00 (+-2g), ACCEL_CONFIG2 0x1D=0x03 (acc DLPF ~44Hz),
// burst 14B từ 0x3B (Acc6+Temp2+Gyro6).
#include "mpu6500.h"
#include "i2c_reg.h"
#include "board.h"

#define MPU_ADDR 0x68
#define R_WHO    0x75
#define R_PWR    0x6B
#define R_DIV    0x19
#define R_CFG    0x1A
#define R_GCFG   0x1B
#define R_ACFG   0x1C
#define R_ACFG2  0x1D
#define R_INTEN  0x38
#define R_OUT    0x3B

static uint8_t s_addr = 0; // địa chỉ thực tế đã dò (0x68 AD0=GND, 0x69 AD0=VCC/floating)

bool mpu6500_init(void) {
  i2c1_init_400k();
  // Reset MCU không reset slave: transaction dở dang trước đó có thể để SDA
  // kẹt thấp → mọi boot sau đều FAIL. Dọn bus trước mỗi init (chuẩn bring-up).
  i2c1_recover();
  board_delay_ms(50);
  // Tự dò địa chỉ: AD0=GND → 0x68, AD0=VCC/float → 0x69 (hay gặp khi dây AD0 rơi)
  static const uint8_t CAND[] = {0x68, 0x69};
  for (uint8_t i = 0; i < sizeof(CAND); i++) {
    if (i2c1_probe(CAND[i])) { s_addr = CAND[i]; break; }
  }
  if (!s_addr) return false;
  // Bring-up thực tế bus hay chập chờn (dây breadboard): retry 3 lần.
  for (int t = 0; t < 3; t++) {
    if (i2c1_write_reg(s_addr, R_PWR, 0x01)) break;
    if (t == 2) return false;
    board_delay_ms(20);
  }
  board_delay_ms(50);
  i2c1_write_reg(s_addr, R_DIV, 0x01);
  i2c1_write_reg(s_addr, R_CFG, 0x03);
  i2c1_write_reg(s_addr, R_GCFG, 0x00);
  i2c1_write_reg(s_addr, R_ACFG, 0x00);
  i2c1_write_reg(s_addr, R_ACFG2, 0x03);
  uint8_t id = 0;
  for (int t = 0; t < 3; t++) {
    if (mpu6500_whoami(&id)) break;
    board_delay_ms(20);
  }
  if (id == 0) return false;
  return id == 0x70; // MPU6500. 0x68 là MPU6050 -> sai module
}

uint8_t mpu6500_addr(void) { return s_addr; }

bool mpu6500_whoami(uint8_t *id) {
  if (!s_addr) return false;
  return i2c1_read_reg(s_addr, R_WHO, id);
}

bool mpu6500_read_raw(int16_t *ax, int16_t *ay, int16_t *az,
                      int16_t *gx, int16_t *gy, int16_t *gz) {
  if (!s_addr) return false;
  uint8_t b[14];
  if (!i2c1_burst_read(s_addr, R_OUT, b, 14)) return false;
  #define RD(i) ((int16_t)(((uint16_t)b[i] << 8) | b[(i)+1]))
  *ax = RD(0); *ay = RD(2); *az = RD(4);
  *gx = RD(8); *gy = RD(10); *gz = RD(12);
  #undef RD
  return true;
}

void mpu6500_get_scale(float *acc_lsb, float *gyro_lsb) {
  if (acc_lsb) *acc_lsb = 16384.0f; // +-2g
  if (gyro_lsb) *gyro_lsb = 131.0f; // +-250dps
}

void mpu6500_enable_data_ready(void) {
  if (!s_addr) return;
  i2c1_write_reg(s_addr, R_INTEN, 0x01); // DATA_RDY_EN
}
