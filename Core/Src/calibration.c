// SV1 - Calibration, baremetal (không Arduino).
#include "calibration.h"
#include "mpu6500.h"
#include "board.h"
#include "usart_reg.h"

GyroBias_t calibration_gyro_bias(void) {
  usart1_write("# Calib gyro - GIU YEN 2s...\r\n");
  int64_t sx = 0, sy = 0, sz = 0;
  const int N = 1000;
  int ok = 0;
  // Đọc tới khi đủ 1000 mẫu (lỗi bus tạm thời không làm calib ra 0), tối đa 6s
  uint32_t t0 = board_millis();
  while (ok < N && (board_millis() - t0) < 6000) {
    int16_t ax, ay, az, gx, gy, gz;
    if (mpu6500_read_raw(&ax, &ay, &az, &gx, &gy, &gz)) {
      sx += gx; sy += gy; sz += gz; ok++;
      board_delay_ms(2); // boot mới được delay, loop chính cấm
    }
  }
  float al, gl;
  mpu6500_get_scale(&al, &gl);
  GyroBias_t b = {0, 0, 0};
  if (ok > 0) {
    b.gx = ((float)sx / (float)ok) / gl;
    b.gy = ((float)sy / (float)ok) / gl;
    b.gz = ((float)sz / (float)ok) / gl;
  }
  usart1_printf("# bias dps gx=%.2f gy=%.2f gz=%.2f (n=%d)\r\n", b.gx, b.gy, b.gz, ok);
  return b;
}

AccCal_t calibration_acc_default(void) {
  AccCal_t c = {0, 0, 0, 1, 1, 1};
  return c;
}

void acc_apply(const AccCal_t *c, float *axg, float *ayg, float *azg) {
  *axg = (*axg - c->ox) * c->sx;
  *ayg = (*ayg - c->oy) * c->sy;
  *azg = (*azg - c->oz) * c->sz;
}
