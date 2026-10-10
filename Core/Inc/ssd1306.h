#pragma once
// SV4 - OLED 0.96" SSD1306 128x64 I2C 0x3C, mức thanh ghi qua i2c_reg.
// Không dùng Adafruit lib. Framebuffer 1KB. Update chia page (1 page/call ~3ms)
// để không phá nhịp 500Hz: gọi oled_update_1page() mỗi tick telemetry 50Hz
// -> full refresh 8 page ~160ms (~6fps), đủ đọc góc.
#include <stdint.h>
#include <stdbool.h>
#define OLED_ADDR 0x3C
bool oled_init(void);          // false nếu không thấy OLED (vẫn chạy tiếp không OLED)
void oled_clear(void);
void oled_puts(uint8_t page, uint8_t x, const char *s); // page 0..7, x 0..127
void oled_printf(uint8_t page, const char *fmt, ...);
void oled_update_1page(void);  // non-blocking: gửi 1 page, xoay vòng 0..7
void oled_update_all(void);    // blocking: gửi cả 8 page (chỉ boot/logo)
bool oled_present(void);
