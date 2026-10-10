// SV4 - Telemetry gói binary, gửi qua ring TX ngắt (xem telemetry.h cho format).
#include "telemetry.h"
#include "usart_reg.h"
#include <string.h>

volatile uint32_t telemetry_pkt_count = 0;

// CRC8 poly 0x07 (ATM) — đơn giản, đủ bắt lỗi byte trên dây
static uint8_t crc8(const uint8_t *d, uint16_t n) {
  uint8_t crc = 0;
  while (n--) {
    crc ^= *d++;
    for (int i = 0; i < 8; i++) {
      crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
    }
  }
  return crc;
}

static void put_u16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put_u32(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
static void put_f32(uint8_t *p, float f) {
  uint32_t u; memcpy(&u, &f, 4); put_u32(p, u); // little-endian, portable
}

void telemetry_banner(void) {
  usart1_write("# BIN pkt AA55|LEN|TS:u32|14xf32|EXEC:u16|DT:u16|rkfix:f32|pkfix:f32|ref:i8|yawr:f32|yawt:f32|CRC8\r\n");
  usart1_write("# f32 order: ax,ay,az,gx,gy,gz,racc,rpacc,rgyro,pgyro,rcf,pcf,rkf,pkf\r\n");
}

void telemetry_send_bin(const SensorData_t *s, const Attitude_t *a, uint32_t exec_us,
                        uint32_t dt_us, float rkfix, float pkfix, float ref_deg) {
  uint8_t b[96];
  uint16_t i = 0;
  b[i++] = TELEMETRY_SYNC0;
  b[i++] = TELEMETRY_SYNC1;
  uint16_t len_pos = i;
  b[i++] = 0; // LEN điền sau
  put_u32(&b[i], s->ts_ms); i += 4;
  #define PF(x) do { put_f32(&b[i], (x)); i += 4; } while (0)
  PF(s->ax); PF(s->ay); PF(s->az);
  PF(s->gx); PF(s->gy); PF(s->gz);
  PF(a->roll_acc);  PF(a->pitch_acc);
  PF(a->roll_gyro); PF(a->pitch_gyro);
  PF(a->roll_cf);   PF(a->pitch_cf);
  PF(a->roll_kalman); PF(a->pitch_kalman);
  #undef PF
  put_u16(&b[i], (uint16_t)exec_us); i += 2;
  put_u16(&b[i], (uint16_t)dt_us);   i += 2;
  put_f32(&b[i], rkfix); i += 4;
  put_f32(&b[i], pkfix); i += 4;
  b[i++] = (uint8_t)(int8_t)ref_deg;
  put_f32(&b[i], a->yaw_raw);  i += 4;
  put_f32(&b[i], a->yaw_tilt); i += 4;
  b[len_pos] = (uint8_t)(i - 3);          // LEN = TS..hết payload
  b[i] = crc8(&b[2], (uint16_t)(i - 2));  // CRC trên LEN + payload
  i++;
  usart1_write_buf(b, i); // non-blocking, ISR TXE gửi dần
  telemetry_pkt_count++;
}
