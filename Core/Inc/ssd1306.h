#pragma once
// SV4 - OLED 0.96" SSD1306 128x64, dùng chung I2C1 PB6/PB7 với QMC5883P.
// Không dùng Adafruit lib. Framebuffer 1KB. Update chia page (1 page/call ~3ms).
// main.c gửi 1 page mỗi 100ms: 8 page hết ~0.8s.
#include <stdint.h>
#include <stdbool.h>
#define OLED_ADDR 0x3C
bool oled_init(void);          // false nếu không thấy OLED (vẫn chạy tiếp không OLED)
void oled_clear(void);
void oled_puts(uint8_t page, uint8_t x, const char *s); // page 0..7, x 0..127
void oled_puts_medium(uint8_t x, uint8_t y, const char *s); // chữ 7x14 cho số góc
void oled_draw_grid(void);   // vạch chia bốn ô 64x32
void oled_printf(uint8_t page, const char *fmt, ...);
uint8_t oled_next_page(void);  // page sẽ được gửi ở lần cập nhật tiếp theo
void oled_update_1page(void);  // non-blocking: gửi 1 page, xoay vòng 0..7
void oled_update_all(void);    // blocking: gửi cả 8 page (chỉ boot/logo)
bool oled_present(void);
