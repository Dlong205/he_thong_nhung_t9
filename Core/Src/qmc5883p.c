// Mở rộng - QMC5883P driver baremetal.
#include "qmc5883p.h"
#include "i2c_reg.h"
#include "board.h"

#define R_ID   0x00
#define R_XL   0x01
#define R_ST   0x09
#define R_C1   0x0A
#define R_C2   0x0B
#define R_CFG3 0x29

static bool s_ok = false;
static uint8_t s_addr = 0; // địa chỉ đã dò được
// Hard-iron offset (raw LSB), mặc định 0
static float s_ox = 0, s_oy = 0, s_oz = 0;
static int16_t s_minx = 32767, s_miny = 32767, s_minz = 32767;
static int16_t s_maxx = -32768, s_maxy = -32768, s_maxz = -32768;
static bool s_has_range = false;

bool qmc_init(void) {
  // Tự dò địa chỉ: nhiều clone P/L để 0x2C/0x0D/0x0C. CHIPID phải = 0x80.
  static const uint8_t CAND[] = {0x2C, 0x0D, 0x0C};
  for (uint8_t i = 0; i < sizeof(CAND); i++) {
    uint8_t id = 0;
    if (i2c1_read_reg(CAND[i], R_ID, &id) && id == 0x80) { s_addr = CAND[i]; break; }
  }
  if (!s_addr) { s_ok = false; return false; }
  // Sequence cộng đồng đã kiểm chứng (Siliqs/DFRobot): 0x29, CTRL2, CTRL1
  i2c1_write_reg(s_addr, R_CFG3, 0x06);
  board_delay_ms(10);
  i2c1_write_reg(s_addr, R_C2, 0x08);   // RANGE ~8G + set/reset on
  board_delay_ms(10);
  if (!i2c1_write_reg(s_addr, R_C1, 0xCD)) { s_ok = false; return false; }
  board_delay_ms(20);
  s_ok = true;
  s_has_range = true;
  return true;
}

bool qmc_present(void) { return s_ok; }

uint8_t qmc_addr(void) { return s_addr; }

bool qmc_data_ready(void) {
  if (!s_ok) return false;
  uint8_t st = 0;
  if (!i2c1_read_reg(s_addr, R_ST, &st)) return false;
  return (st & 0x01) != 0;
}

bool qmc_read_raw(int16_t *mx, int16_t *my, int16_t *mz) {
  uint8_t b[6];
  if (!s_ok) return false;
  if (!i2c1_burst_read(s_addr, R_XL, b, 6)) return false;
  // Little-endian (ngược MPU big-endian — bẫy hay gặp khi copy code)
  int16_t x = (int16_t)(((uint16_t)b[1] << 8) | b[0]);
  int16_t y = (int16_t)(((uint16_t)b[3] << 8) | b[2]);
  int16_t z = (int16_t)(((uint16_t)b[5] << 8) | b[4]);
  if (mx) *mx = x;
  if (my) *my = y;
  if (mz) *mz = z;
  return true;
}

void qmc_get_gauss_scale(float *lsb_per_gauss) {
  // RANGE 8G: 32768/8 = 4096 LSB/Gauss
  if (lsb_per_gauss) *lsb_per_gauss = 4096.0f;
  (void)s_has_range;
}

void qmc_calib_reset(void) {
  s_minx = s_miny = s_minz = 32767;
  s_maxx = s_maxy = s_maxz = -32768;
}

void qmc_calib_collect(int16_t mx, int16_t my, int16_t mz) {
  if (mx < s_minx) s_minx = mx;
  if (mx > s_maxx) s_maxx = mx;
  if (my < s_miny) s_miny = my;
  if (my > s_maxy) s_maxy = my;
  if (mz < s_minz) s_minz = mz;
  if (mz > s_maxz) s_maxz = mz;
}

bool qmc_calib_finish(void) {
  int32_t rx = (int32_t)s_maxx - s_minx;
  int32_t ry = (int32_t)s_maxy - s_miny;
  // Phải xoay đủ vòng: biên X/Y > 500 LSB (~0.12G) mới tin được
  if (rx < 500 || ry < 500) return false;
  s_ox = ((float)s_maxx + s_minx) * 0.5f;
  s_oy = ((float)s_maxy + s_miny) * 0.5f;
  s_oz = ((float)s_maxz + s_minz) * 0.5f;
  return true;
}

void qmc_calib_get(float *ox, float *oy, float *oz) {
  if (ox) *ox = s_ox;
  if (oy) *oy = s_oy;
  if (oz) *oz = s_oz;
}
