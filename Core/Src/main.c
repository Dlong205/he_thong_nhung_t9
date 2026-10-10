// K03 Đề Khó - baremetal STM32Cube + thanh ghi (không Arduino).
// SV1: board/i2c_reg/exti/usart_reg | SV2: mpu6500/calib/attitude/cf/kalman(+fix)/timing
// SV3: servo_ref (bàn đối chứng) | SV4: telemetry + ssd1306 + tích hợp.
// Mở rộng: qmc5883p + yaw tilt-compensated.
// Luồng: EXTI PA0 DATA_RDY -> flag -> loop xử lý mẫu 500Hz, telemetry+mag 50Hz, OLED 10Hz.
// CẤM HAL_Delay trong loop chính (chỉ dùng ở boot/calib).
#include "stm32f1xx_hal.h"
#include "board.h"
#include "timing.h"
#include "usart_reg.h"
#include "i2c_reg.h"
#include "mpu6500.h"
#include "calibration.h"
#include "attitude.h"
#include "complementary.h"
#include "complementary_fix.h"
#include "kalman.h"
#include "kalman_fix.h"
#include "exti.h"
#include "telemetry.h"
#include "servo_ref.h"
#include "ssd1306.h"
#include "qmc5883p.h"
#include "yaw.h"
#include "types.h"
#include <string.h>
#include <stdlib.h>

#define USE_EXTI 1
#define USART_BAUD 115200u  // đủ cho gói binary 85B@50Hz (4.3KB/s). Full 500Hz thì đổi 460800.
// TẠM ẨN để kiểm tra lõi: 0 = bỏ servo + các test (SEQ/REF/MAGCAL), chỉ chạy
// MPU + CF/Kalman + OLED + mag hiển thị. Muốn bật lại test: đổi thành 1.
#define USE_SERVO 1
#define USE_MAGCAL 1

static GyroBias_t gbias;
static AccCal_t accal;
static GyroInt_t gint;
static CF_t cf;
static CFfix_t cffix;
static Kalman_t kr, kp;
static KalmanFix_t krf, kpf;

static bool s_has_oled = false;
static bool s_bin_on = true; // BIN 0 = tạm dừng gói binary (console sạch để gõ lệnh)
static bool s_has_mag = false;
#if USE_MAGCAL
// MAGCAL: thu hard-iron 15s (xoay ngang 360°), không chặn loop
static bool s_magcal = false;
static uint32_t s_magcal_t0 = 0;
#endif

static uint32_t dwt_to_us(uint32_t c0, uint32_t c1) {
  uint32_t mhz = HAL_RCC_GetSysClockFreq() / 1000000u;
  return (c1 - c0) / mhz;
}

int main(void) {
  board_init();
  timing_init();
  usart1_init(USART_BAUD);
  usart1_write("\r\n# K03 baremetal Cube+REG+OLED+MAG boot\r\n");

  // I2C mở trước mọi module (OLED cần bus để báo trạng thái kể cả khi MPU chết)
  i2c1_init_400k();
  i2c1_recover(); // dọn slave kẹt sau reset-MCU (SDA thấp) — bắt buộc bring-up
  s_has_oled = oled_init();
  usart1_printf("# OLED %s\r\n", s_has_oled ? "OK" : "ABSENT (chay tiep, khong OLED)");
  if (s_has_oled) { oled_printf(0, "K03 BOOT..."); oled_printf(1, "WAIT MPU"); oled_update_all(); }

  // Quét nhanh bus, HIỆN LÊN OLED để bring-up không cần UART/máy tính:
  // 3C=OLED, 68=MPU(AD0=GND), 69=MPU(AD0=hở/VCC), 2C/0C=la bàn clone
  if (s_has_oled) {
    int p3c = i2c1_probe(0x3C), p68 = i2c1_probe(0x68), p69 = i2c1_probe(0x69);
    int p2c = i2c1_probe(0x2C), p0c = i2c1_probe(0x0C);
    oled_printf(0, "SCAN BUS I2C1");
    oled_printf(1, "3C:%d 68:%d 69:%d", p3c, p68, p69);
    oled_printf(2, "2C:%d 0C:%d", p2c, p0c);
    oled_printf(3, "68/69 = MPU pin");
    oled_update_all();
    usart1_printf("# SCAN 3C:%d 68:%d 69:%d 2C:%d 0C:%d\r\n", p3c, p68, p69, p2c, p0c);
    board_delay_ms(3000); // giữ màn 3s cho user đọc
  }

  // MPU bắt buộc nhưng KHÔNG fail vĩnh viễn: quét lại liên tục (LED nháy = còn
  // sống, màn hiện số lần thử + bus scan). Cắm lại dây là tự hồi trong ≤1s.
  uint8_t who = 0;
  uint32_t attempt = 0;
  while (!(mpu6500_init() && mpu6500_whoami(&who) && who == 0x70)) {
    attempt++;
    board_led_toggle();
    if (s_has_oled && (attempt % 5) == 1) {
      oled_printf(0, "WAIT MPU n=%lu", (unsigned long)attempt);
      oled_printf(1, "VCC/SCL/SDA lai?");
      oled_printf(2, "NCS->3V3 AD0->GND");
      oled_printf(3, "7bit 3C:%d 68:%d 69:%d",
                  (int)i2c1_probe(0x3C), (int)i2c1_probe(0x68), (int)i2c1_probe(0x69));
      oled_update_all();
    }
    board_delay_ms(200);
  }
  usart1_printf("# WHO_AM_I=0x%02X addr=0x%02X (attempt=%lu)\r\n", who, mpu6500_addr(), (unsigned long)attempt);
  if (s_has_oled) {
    oled_printf(0, "MPU OK 0x%02X @0x%02X", who, mpu6500_addr());
    oled_printf(1, "K03 RUNNING...");
    oled_update_all();
    board_delay_ms(700); // giữ màn báo OK ngắn cho user thấy
  }

  // Mở rộng: không có mag vẫn chạy tiếp (báo rõ để chấm đúng)
  s_has_mag = qmc_init();
  usart1_printf("# QMC5883P %s\r\n", s_has_mag ? "OK" : "ABSENT (chay tiep, yaw=0)");
  if (s_has_mag) usart1_printf("# QMC addr=0x%02X\r\n", qmc_addr());
  if (s_has_oled) {
    oled_printf(0, "K03 BOOT OK");
    oled_printf(1, "MPU 0x%02X MAG %d", who, s_has_mag ? 1 : 0);
    oled_update_all();
  }

#if USE_SERVO
  servoref_init();
#else
  usart1_write("# SERVO OFF (tam an de kiem tra loi)\r\n");
#endif
#if USE_EXTI
  mpu6500_enable_data_ready();
  exti_init_pa0();
#endif

  kalman_init(&kr); kalman_init(&kp);
  kfix_init(&krf); kfix_init(&kpf);
  cf_init(&cf, CF_ALPHA_DEFAULT);
  cffix_init(&cffix, CF_ALPHA_DEFAULT);
  gyro_int_reset(&gint);

  // Calib gyro ở boot (được delay, không phải loop chính)
  gbias = calibration_gyro_bias();
  // Acc 6-mặt 2026-09-28 (pc/acc_calib.py, đã remap X/Y do tên file user đặt nhầm)
  accal.ox = -0.00520f; accal.oy = 0.01414f; accal.oz = -0.01696f;
  accal.sx = 1.01392f;  accal.sy = 1.00266f;
  accal.sz = 0.99703f;

  i2c1_dump_regs();
  usart1_dump_regs();
#if USE_EXTI
  exti_dump();
#endif
  telemetry_banner();

  uint32_t last_dwt = timing_cycles();
  uint32_t cnt = 0, int_ok = 0, int_timeout = 0;
  char cmd[96];
  float yaw_r = 0, yaw_t = 0;

  while (1) {
    // Lệnh non-blocking: OLED status luôn bật; REF/SEQ + MAGCAL chỉ khi mở test
    while (usart1_getline(cmd, sizeof(cmd))) {
#if USE_SERVO
      if (!servoref_parse(cmd)) {
#endif
#if USE_MAGCAL
        if (strncmp(cmd, "MAGCAL", 6) == 0) {
          if (s_has_mag) {
            s_magcal = true; s_magcal_t0 = board_millis();
            qmc_calib_reset();
            usart1_write("# MAGCAL start: xoay ngang 360d/15s\r\n");
          } else usart1_write("# MAGCAL: khong co mag\r\n");
        } else
#endif
        if (strncmp(cmd, "OLED", 4) == 0) {
          usart1_printf("# OLED present=%d MAG present=%d yaw=%.0f/%.0f\r\n",
                        s_has_oled ? 1 : 0, s_has_mag ? 1 : 0, yaw_r, yaw_t);
        } else if (strncmp(cmd, "BIN", 3) == 0) {
          s_bin_on = (cmd[3] != '0'); // "BIN 0" tắt stream, còn lại bật
          usart1_printf("# BIN %d\r\n", s_bin_on ? 1 : 0);
        }
#if USE_SERVO
      }
#endif
    }
#if USE_SERVO
    servoref_update();
#endif
#if USE_MAGCAL
    // MAGCAL tự kết thúc sau 15s
    if (s_magcal && board_millis() - s_magcal_t0 > 15000) {
      s_magcal = false;
      if (qmc_calib_finish()) {
        float ox, oy, oz;
        qmc_calib_get(&ox, &oy, &oz);
        usart1_printf("# MAGCAL OK ox=%.0f oy=%.0f oz=%.0f LSB\r\n", ox, oy, oz);
      } else usart1_write("# MAGCAL FAIL: xoay chua du bien (>500 LSB X/Y)\r\n");
    }
#endif

    uint32_t now_cyc = timing_cycles();
    uint32_t dt_us = dwt_to_us(last_dwt, now_cyc);
#if USE_EXTI
    if (exti_consume()) { int_ok++; }
    else {
      // Fallback nếu mất INT >5ms (tránh treo khi chưa cắm dây PA0)
      if (dt_us < 5000) continue;
      int_timeout++;
    }
#else
    if (dt_us < 2000) continue;
#endif
    last_dwt = now_cyc;
    uint32_t c0 = timing_cycles();

    int16_t ax, ay, az, gx, gy, gz;
    // Đọc MPU có retry (bus breadboard hay chập chờn). Lỗi kéo dài thì BÁO lên
    // OLED thay vì treo cả hiển thị như bản cũ (continue mù).
    static uint32_t mpu_err = 0;
    bool have_sample = false;
    for (int t = 0; t < 3; t++) {
      if (mpu6500_read_raw(&ax, &ay, &az, &gx, &gy, &gz)) { have_sample = true; break; }
    }
    if (!have_sample) {
      mpu_err++;
      // Bus treo (slave giữ SDA sau glitch): tự cứu mỗi 150 mẫu lỗi
      if ((mpu_err % 150) == 0) i2c1_recover();
      // Báo lỗi ~10Hz (không spam bus): vẫn đẩy 1 page OLED để người dùng thấy
      if (s_has_oled && (mpu_err % 50) == 1) {
        oled_printf(0, "MPU READ ERR");
        oled_printf(1, "CHK VCC/SDA/SCL");
        oled_printf(2, "fails=%lu", (unsigned long)mpu_err);
        oled_printf(3, "I2C fail=%d", i2c_fail_at);
        oled_update_1page();
      }
      continue;
    }
    if (mpu_err > 0 && s_has_oled) {
      // Vừa phục hồi sau chuỗi lỗi: hiện lại số liệu ngay
      mpu_err = 0;
    }
    float al, gl;
    mpu6500_get_scale(&al, &gl);
    SensorData_t s;
    s.ax = (float)ax / al; s.ay = (float)ay / al; s.az = (float)az / al;
    acc_apply(&accal, &s.ax, &s.ay, &s.az);
    s.gx = (float)gx / gl - gbias.gx;
    s.gy = (float)gy / gl - gbias.gy;
    s.gz = (float)gz / gl - gbias.gz;
    s.mx = 0; s.my = 0; s.mz = 0;
    s.dt = SYS_TS;
    s.ts_ms = board_millis();

    Attitude_t a;
    attitude_acc(&s, &a.roll_acc, &a.pitch_acc);
    gyro_int_update(&gint, &s);
    a.roll_gyro = gint.roll; a.pitch_gyro = gint.pitch;
    cf_update(&cf, a.roll_acc, a.pitch_acc, s.gx, s.gy, s.dt);
    a.roll_cf = cf.roll; a.pitch_cf = cf.pitch;
    cffix_update(&cffix, a.roll_acc, a.pitch_acc, s.gx, s.gy, s.dt);
    a.roll_kalman = kalman_update(&kr, a.roll_acc, s.gx, s.dt);
    a.pitch_kalman = kalman_update(&kp, a.pitch_acc, s.gy, s.dt);
    float rkfix = kfix_update(&krf, a.roll_acc, s.gx, s.dt);
    float pkfix = kfix_update(&kpf, a.pitch_acc, s.gy, s.dt);
    a.yaw_raw = yaw_r; a.yaw_tilt = yaw_t;

    uint32_t exec_us = dwt_to_us(c0, timing_cycles());
    cnt++;
    if (cnt % SYS_TELEMETRY_DIV == 0) {
      // Mag 50Hz (ODR chip 200Hz, đọc decimate đủ): poll DRDY rồi burst 6B LE
      if (s_has_mag && qmc_data_ready()) {
        int16_t mrx, mry, mrz;
        if (qmc_read_raw(&mrx, &mry, &mrz)) {
#if USE_MAGCAL
          if (s_magcal) qmc_calib_collect(mrx, mry, mrz);
#endif
          float ox, oy, oz;
          qmc_calib_get(&ox, &oy, &oz);
          float fx = (float)mrx - ox, fy = (float)mry - oy, fz = (float)mrz - oz;
          float lsb;
          qmc_get_gauss_scale(&lsb);
          s.mx = fx / lsb; s.my = fy / lsb; s.mz = fz / lsb;
          yaw_r = yaw_wrap180(yaw_raw(fx, fy));
          yaw_t = yaw_wrap180(yaw_tilt(fx, fy, fz, a.roll_kalman, a.pitch_kalman));
          a.yaw_raw = yaw_r; a.yaw_tilt = yaw_t;
        }
      }
      board_led_toggle();
      if (s_bin_on) {
#if USE_SERVO
        telemetry_send_bin(&s, &a, exec_us, dt_us, rkfix, pkfix, servoref_get());
#else
        telemetry_send_bin(&s, &a, exec_us, dt_us, rkfix, pkfix, 0.0f);
#endif
      }
      // OLED 10Hz: VIẾT LẠI chữ mỗi tick (số liệu tươi 10Hz), gửi 1 page/tick
      // (full refresh 8 page ~0.8s). Bản cũ chỉ viết chữ mỗi 800ms nên tưởng đơ.
      if (s_has_oled && (cnt % (SYS_TELEMETRY_DIV * 5)) == 0) {
        oled_printf(0, "R%+.1f P%+.1f", a.roll_kalman, a.pitch_kalman);
        oled_printf(1, "Y%+.0f T%+.0f", yaw_r, yaw_t);
#if USE_SERVO
        oled_printf(2, "REF%+.0f %s", servoref_get(), servoref_seqname());
#else
        oled_printf(2, "CORE OK 500Hz");
#endif
        oled_printf(3, "E%lu D%lu %s", (unsigned long)exec_us,
                    (unsigned long)dt_us, s_has_mag ? "MAG" : "NOMAG");
        oled_update_1page();
      }
      if (cnt % 250 == 0) {
#if USE_EXTI
#if USE_SERVO
        usart1_printf("# exti ok=%lu timeout=%lu seq=%s yaw=%.0f/%.0f\r\n",
                      (unsigned long)int_ok, (unsigned long)int_timeout,
                      servoref_seqname(), yaw_r, yaw_t);
#else
        usart1_printf("# exti ok=%lu timeout=%lu yaw=%.0f/%.0f\r\n",
                      (unsigned long)int_ok, (unsigned long)int_timeout,
                      yaw_r, yaw_t);
#endif
#endif
      }
    }
  }
}
