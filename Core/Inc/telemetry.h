#pragma once
#include "types.h"
// SV4 - Telemetry GÓI BINARY (theo gợi ý đề: HEADER|LENGTH|TIMESTAMP|DATA|CHECKSUM).
// Khung (little-endian):
//   0xAA 0x55 | LEN(u8) | TS(u32) | 14×float32 | EXEC(u16) | DT(u16) |
//   rkfix(f32) | pkfix(f32) | ref(i8) | yawr(f32) | yawt(f32) | CRC8
// 14 float theo thứ tự: ax,ay,az, gx,gy,gz, racc,rpacc, rgyro,pgyro, rcf,pcf, rkf,pkf
// LEN = số byte từ TS đến hết payload (81). CRC8 poly 0x07 trên LEN+payload.
// Không dùng printf float cho đường telemetry → ngắt TX ring, không phá 500Hz.
#define TELEMETRY_SYNC0 0xAA
#define TELEMETRY_SYNC1 0x55
extern volatile uint32_t telemetry_pkt_count; // đếm gói đã gửi (kiểm tra qua ST-Link)
void telemetry_banner(void);
void telemetry_send_bin(const SensorData_t *s, const Attitude_t *a, uint32_t exec_us,
                        uint32_t dt_us, float rkfix, float pkfix, float ref_deg);
