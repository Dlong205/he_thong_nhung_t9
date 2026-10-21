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
#include "i2c2_reg.h"
#include "mpu6500.h"
#include "calibration.h"
#include "attitude.h"
#include "complementary.h"
#include "complementary_fix.h"
#include "kalman.h"
#include "exti.h"
#include "telemetry.h"
#include "servo_ref.h"
#include "ssd1306.h"
#include "qmc5883p.h"
#include "yaw.h"
#include "types.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define USE_EXTI 1
#define USART_BAUD 115200u  // đủ cho gói binary 77B@50Hz (3.85KB/s).
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

static bool s_has_oled = false;
static bool s_bin_on = true; // BIN 0 = tạm dừng gói binary (console sạch để gõ lệnh)
static bool s_has_mag = false;
// Giữ kết quả quét trong RAM để đọc qua ST-Link khi UART hoặc OLED im lặng.
// Thứ tự: OLED 3C/3D, MPU 68/69, QMC 2C/0D/0C.
volatile uint8_t i2c_scan_result[7];
volatile uint8_t i2c_mpu_whoami_last = 0;
volatile uint8_t i2c_mpu_init_ok = 0;
volatile uint32_t mpu_read_fail_total = 0;
#if USE_MAGCAL
// MAGCAL: thu hard-iron 15s (xoay ngang 360°), không chặn loop
static bool s_magcal = false;
static uint32_t s_magcal_t0 = 0;
#endif

static uint32_t dwt_to_us(uint32_t c0, uint32_t c1) {
  uint32_t mhz = HAL_RCC_GetSysClockFreq() / 1000000u;
  return (c1 - c0) / mhz;
}

// Mỗi ô 64x32px: nhãn nhỏ, số góc 7x14px ở giữa ô.
static void oled_angle_cell(uint8_t col, uint8_t row, const char *name,
                            float angle) {
  char value[12];
  snprintf(value, sizeof(value), "%+.2f", (double)angle);
  uint8_t len = (uint8_t)strlen(value);
  if (len > 7) { strcpy(value, "OVR"); len = 3; }
  uint8_t x0 = col ? 64 : 0;
  uint8_t y0 = row ? 32 : 0;
  oled_puts(y0 / 8, x0 + 5, name);
  oled_puts_medium(x0 + (uint8_t)((63 - len * 8) / 2), y0 + 12, value);
}

int main(void) {
  board_init();
  timing_init();
  usart1_init(USART_BAUD);
  usart1_write("\r\n# K03 baremetal Cube+REG+OLED+MAG boot\r\n");

  // Hai bus độc lập: OLED+QMC trên I2C1 PB6/PB7; MPU trên I2C2 PB10/PB11.
  i2c1_init_400k();
  i2c1_recover();
  i2c2_init_100k();
  i2c2_recover();
  s_has_oled = oled_init();
  usart1_printf("# OLED %s\r\n", s_has_oled ? "OK" : "ABSENT (chay tiep, khong OLED)");
  if (s_has_oled) { oled_printf(0, "K03 BOOT..."); oled_printf(1, "WAIT MPU"); oled_update_all(); }

  // Quét từng bus riêng và báo rõ thiết bị nào lỗi.
  int p3c = i2c1_probe(0x3C), p3d = i2c1_probe(0x3D);
  int p68 = i2c2_probe(0x68), p69 = i2c2_probe(0x69);
  int p2c = i2c1_probe(0x2C), p0d = i2c1_probe(0x0D), p0c = i2c1_probe(0x0C);
  i2c_scan_result[0] = (uint8_t)p3c;
  i2c_scan_result[1] = (uint8_t)p3d;
  i2c_scan_result[2] = (uint8_t)p68;
  i2c_scan_result[3] = (uint8_t)p69;
  i2c_scan_result[4] = (uint8_t)p2c;
  i2c_scan_result[5] = (uint8_t)p0d;
  i2c_scan_result[6] = (uint8_t)p0c;
  usart1_printf("# BUS1 PB6/7 OLED 3C:%d 3D:%d MAG 2C:%d 0D:%d 0C:%d | BUS2 PB10/11 MPU 68:%d 69:%d\r\n",
                p3c, p3d, p2c, p0d, p0c, p68, p69);
  if (s_has_oled) {
    oled_printf(0, "2 BUS ISOLATED");
    oled_printf(1, "B1 O:%d/%d MAG:%d", p3c, p3d, p2c);
    oled_printf(2, "B2 MPU:%d/%d", p68, p69);
    oled_printf(3, "PB6/7 | PB10/11");
    oled_update_all();
    board_delay_ms(3000); // giữ màn 3s cho user đọc
  }

  // QMC nằm trên I2C1 cùng OLED: xác minh CHIPID ngay cả khi MPU trên I2C2 lỗi.
  s_has_mag = qmc_init();
  usart1_printf("# QMC5883P %s addr=0x%02X\r\n",
                s_has_mag ? "OK" : "ABSENT", qmc_addr());

  // MPU bắt buộc nhưng KHÔNG fail vĩnh viễn: quét lại liên tục (LED nháy = còn
  // sống, màn hiện số lần thử + bus scan). Cắm lại dây là tự hồi trong ≤1s.
  uint8_t who = 0;
  uint32_t attempt = 0;
  for (;;) {
    bool init_ok = mpu6500_init();
    bool who_ok = init_ok && mpu6500_whoami(&who);
    i2c_mpu_init_ok = (uint8_t)init_ok;
    i2c_mpu_whoami_last = who_ok ? who : 0;
    if (init_ok && who_ok && who == 0x70) break;
    attempt++;
    board_led_toggle();
    if (s_has_oled && (attempt % 5) == 1) {
      oled_printf(0, "WAIT MPU n=%lu", (unsigned long)attempt);
      oled_printf(1, "VCC/SCL/SDA lai?");
      oled_printf(2, "NCS->3V3 AD0->GND");
      oled_printf(3, "I2C2 68:%d 69:%d",
                  (int)i2c2_probe(0x68), (int)i2c2_probe(0x69));
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

  // QMC đã được kiểm tra trên I2C1 trước vòng đợi MPU ở I2C2.
  if (s_has_oled) {
    oled_printf(0, "K03 BOOT OK");
    oled_printf(1, "MPU 0x%02X MAG %d", who, s_has_mag ? 1 : 0);
    oled_update_all();
  }

#if USE_SERVO
  servoref_init();
  // PWM vừa bật có thể kéo servo từ góc cũ về 0 và rung giá MPU.
  // Đợi cơ cấu đứng yên rồi mới lấy gyro bias; chỉ chặn trong boot.
  board_delay_ms(1500);
#else
  usart1_write("# SERVO OFF (tam an de kiem tra loi)\r\n");
#endif
#if USE_EXTI
  mpu6500_enable_data_ready();
  exti_init_pa0();
#endif

  kalman_init(&kr); kalman_init(&kp);
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
  i2c2_dump_regs();
  usart1_dump_regs();
#if USE_EXTI
  exti_dump();
#endif
  telemetry_banner();

  uint32_t last_dwt = timing_cycles();
  uint32_t cnt = 0, int_ok = 0, int_timeout = 0;
  char cmd[96];
  float yaw_r = 0, yaw_t = 0;
  uint32_t mag_last_ok_ms = 0, mag_sample_count = 0, mag_read_errors = 0;
  bool mag_calibrated = false;

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
            mag_calibrated = false;
            qmc_calib_reset();
            usart1_write("# MAGCAL start: xoay ngang 360d/15s\r\n");
          } else usart1_write("# MAGCAL: khong co mag\r\n");
        } else
#endif
        if (strncmp(cmd, "OLED", 4) == 0) {
          usart1_printf("# OLED present=%d MAG present=%d yaw=%.0f/%.0f\r\n",
                        s_has_oled ? 1 : 0, s_has_mag ? 1 : 0, yaw_r, yaw_t);
        } else if (strncmp(cmd, "BIN", 3) == 0) {
          const char *arg = cmd + 3;
          while (*arg == ' ') arg++;
          if (arg[0] == '0' && arg[1] == '\0') s_bin_on = false;
          else if (arg[0] == '1' && arg[1] == '\0') s_bin_on = true;
          else { usart1_write("# usage: BIN 0|1\r\n"); continue; }
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
        mag_calibrated = true;
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
      mpu_read_fail_total++;
      if (mpu_err == 1) {
        usart1_printf("# MPU READ ERR t=%lu i2c2=%u idx=%u sr1=%04lX sr2=%04lX pb=%04lX total=%lu\r\n",
                      (unsigned long)board_millis(), (unsigned)i2c2_fail_at,
                      (unsigned)i2c2_fail_idx, (unsigned long)i2c2_sr1_last,
                      (unsigned long)i2c2_sr2_last,
                      (unsigned long)GPIOB->IDR,
                      (unsigned long)mpu_read_fail_total);
      }
      // Bus treo (slave giữ SDA sau glitch): tự cứu mỗi 150 mẫu lỗi
      if ((mpu_err % 150) == 0) i2c2_recover();
      // Báo lỗi ~10Hz (không spam bus): vẫn đẩy 1 page OLED để người dùng thấy
      if (s_has_oled && (mpu_err % 50) == 1) {
        oled_clear(); // bỏ số đo cũ khi cảm biến ngừng trả dữ liệu
        oled_printf(0, "MPU READ ERR");
        oled_printf(1, "CHK VCC/SDA/SCL");
        oled_printf(2, "fails=%lu", (unsigned long)mpu_err);
        oled_printf(3, "I2C2 fail=%d", i2c2_fail_at);
        oled_update_1page();
      }
      continue;
    }
    if (mpu_err > 0) {
      usart1_printf("# MPU RECOVER t=%lu lost=%lu total=%lu\r\n",
                    (unsigned long)board_millis(), (unsigned long)mpu_err,
                    (unsigned long)mpu_read_fail_total);
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
          mag_last_ok_ms = board_millis();
          mag_sample_count++;
        } else {
          mag_read_errors++;
        }
      }
      bool mag_fresh = s_has_mag && mag_sample_count &&
                       (uint32_t)(board_millis() - mag_last_ok_ms) < 1000u;
      board_led_toggle();
      if (s_bin_on) {
#if USE_SERVO
        telemetry_send_bin(&s, &a, exec_us, dt_us, servoref_get());
#else
        telemetry_send_bin(&s, &a, exec_us, dt_us, 0.0f);
#endif
      }
      // 10 page/s, đủ 8 page trong 0.8s. Chốt dữ liệu ở đầu mỗi vòng
      // để hai nửa của chữ lớn không lấy hai giá trị khác nhau.
      if (s_has_oled && (cnt % (SYS_TELEMETRY_DIV * 5)) == 0) {
        if (oled_next_page() == 0) {
          oled_clear();
          oled_angle_cell(0, 0, "ROLL", a.roll_kalman);
          oled_angle_cell(1, 0, "PITCH", a.pitch_kalman);
          if (mag_fresh) oled_angle_cell(0, 1, "YAW", yaw_t);
          else { oled_puts(4, 5, "YAW OFF");
                 oled_puts_medium(19, 44, "---"); }
#if USE_SERVO
          oled_angle_cell(1, 1, "REF", servoref_get());
#else
          oled_puts(4, 69, "CORE");
          oled_puts_medium(88, 44, "OK");
#endif
          oled_draw_grid();
        }
        oled_update_1page();
      }
      if (cnt % 250 == 0) {
        usart1_printf("# mag present=%d fresh=%d age_ms=%lu cal=%d samples=%lu errors=%lu\r\n",
                      s_has_mag ? 1 : 0, mag_fresh ? 1 : 0,
                      (unsigned long)(mag_sample_count ? board_millis() - mag_last_ok_ms : 0),
                      mag_calibrated ? 1 : 0, (unsigned long)mag_sample_count,
                      (unsigned long)mag_read_errors);
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
