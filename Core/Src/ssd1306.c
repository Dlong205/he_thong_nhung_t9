// SV4 - SSD1306 driver baremetal. Init sequence chuẩn Solomon Systech.
#include "ssd1306.h"
#include "i2c_reg.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

// Font 5x7, ASCII 32..127 (96 ký tự x 5 byte). Classic public-domain font.
static const uint8_t FONT[96][5] = {
{0,0,0,0,0},{0,0,95,0,0},{0,7,0,7,0},{20,127,20,127,20},{36,42,127,42,18},
{35,19,8,100,98},{54,73,86,32,80},{0,8,7,3,0},{0,28,34,65,0},{0,65,34,28,0},
{42,28,127,28,42},{8,8,62,8,8},{0,128,112,48,0},{8,8,8,8,8},{0,0,96,96,0},
{32,16,8,4,2},{62,81,73,69,62},{0,66,127,64,0},{114,73,73,73,70},{33,65,73,77,51},
{24,20,18,127,16},{39,69,69,69,57},{60,74,73,73,49},{65,33,17,9,7},{54,73,73,73,54},
{70,73,73,41,30},{0,0,20,0,0},{0,64,52,0,0},{0,8,20,34,65},{20,20,20,20,20},
{0,65,34,20,8},{2,1,89,9,6},{62,65,93,89,78},{124,18,17,18,124},{127,73,73,73,54},
{62,65,65,65,34},{127,65,65,65,62},{127,73,73,73,65},{127,9,9,9,1},{62,65,65,81,115},
{127,8,8,8,127},{0,65,127,65,0},{32,64,65,63,1},{127,8,20,34,65},{127,64,64,64,64},
{127,2,28,2,127},{127,4,8,16,127},{62,65,65,65,62},{127,9,9,9,6},{62,65,81,33,94},
{127,9,25,41,70},{38,73,73,73,50},{3,1,127,1,3},{63,64,64,64,63},{31,32,64,32,31},
{63,64,56,64,63},{99,20,8,20,99},{3,4,120,4,3},{97,89,73,77,67},{0,127,65,65,65},
{2,4,8,16,32},{0,65,65,65,127},{4,2,1,2,4},{64,64,64,64,64},{0,3,7,8,0},
{32,84,84,120,64},{127,40,68,68,56},{56,68,68,68,40},{56,68,68,40,127},{56,84,84,84,24},
{0,8,126,9,2},{24,164,164,156,120},{127,8,4,4,120},{0,68,125,64,0},{32,64,64,61,0},
{127,16,40,68,0},{0,65,127,64,0},{124,4,120,4,120},{124,8,4,4,120},{56,68,68,68,56},
{252,24,36,36,24},{24,36,36,24,252},{124,8,4,4,8},{72,84,84,84,36},{4,4,63,68,36},
{60,64,64,32,124},{28,32,64,32,28},{60,64,48,64,60},{68,40,16,40,68},{76,144,144,144,124},
{68,100,84,76,68},{0,8,54,65,0},{0,0,119,0,0},{0,65,54,8,0},{2,1,2,4,2},
{60,38,35,38,60}
};

static uint8_t s_fb[8][128];
static bool s_ok = false;
static uint8_t s_next_page = 0;
static uint8_t s_oaddr = OLED_ADDR; // 0x3C, fallback 0x3D (jumper SA0)

// Module 0.96" dùng chip SH1106 (RAM 132 cột, hiển thị bắt đầu từ cột 2) thì phải
// bù 2 cột, nếu không chữ bị khuất mép trái. SSD1306 thuần thì đổi về 0.
// Đây là khác biệt chip, không phải lỗi code.
#define OLED_COL_OFFSET 2

static bool cmds(const uint8_t *c, uint16_t n) {
  // Gộp control 0x00 + n lệnh trong 1 transaction (tối đa ~16 byte/lần)
  uint8_t b[24];
  if (n + 1 > sizeof(b)) return false;
  b[0] = 0x00;
  memcpy(&b[1], c, n);
  return i2c1_write_buf(s_oaddr, b, n + 1);
}

bool oled_init(void) {
  // Thử đọc: SSD1306 không có register đọc hữu ích, thử gửi display-off xem ACK
  static const uint8_t INIT[] = {
    0xAE,             // display off
    0xD5, 0x80,       // clk div
    0xA8, 0x3F,       // mux 64
    0xD3, 0x00,       // offset
    0x40,             // start line
    0x8D, 0x14,       // charge pump enable
    0x20, 0x02,       // page addressing (SH1106 bỏ qua; SSD1306 chọn page mode)
    0xA1,             // seg remap
    0xC8,             // com scan dec
    0xDA, 0x12,       // com config
    0x81, 0xCF,       // contrast
    0xD9, 0xF1,       // precharge
    0xDB, 0x40,       // vcom
    0xA4,             // resume RAM
    0xA6,             // normal (không invert)
    0xAF              // display on
  };
  // Gửi từng cụm nhỏ để tránh buffer I2C quá dài. Thử 0x3C trước, fallback 0x3D.
  for (uint8_t t = 0; t < 2; t++) {
    s_oaddr = (t == 0) ? OLED_ADDR : (OLED_ADDR | 0x01);
    bool ok = true;
    for (uint16_t i = 0; i < sizeof(INIT);) {
      uint16_t n = sizeof(INIT) - i;
      if (n > 8) n = 8;
      if (!cmds(&INIT[i], n)) { ok = false; break; }
      i += n;
    }
    if (ok) break;
    if (t == 1) { s_ok = false; return false; }
  }
  s_ok = true;
  oled_clear();
  oled_update_all();
  return true;
}

bool oled_present(void) { return s_ok; }

void oled_clear(void) {
  memset(s_fb, 0, sizeof(s_fb));
}

static void draw_char(uint8_t page, uint8_t x, char c) {
  if (page > 7 || x + 5 > 128) return;
  if (c < 32 || c > 127) c = '?';
  const uint8_t *g = FONT[(uint8_t)(c - 32)];
  for (int i = 0; i < 5; i++) s_fb[page][x + i] = g[i];
  if (x + 5 < 128) s_fb[page][x + 5] = 0; // cách 1 cột
}

void oled_puts(uint8_t page, uint8_t x, const char *s) {
  if (!s) return;
  while (*s && x + 5 < 128) {
    draw_char(page, x, *s++);
    x += 6;
  }
}

void oled_printf(uint8_t page, const char *fmt, ...) {
  char buf[22]; // 128px / 6px = 21 ký tự/dòng
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  // Xóa dòng trước khi ghi để không còn chữ cũ
  memset(s_fb[page], 0, 128);
  oled_puts(page, 0, buf);
}

static bool set_pos(uint8_t page, uint8_t col) {
  uint8_t c = (uint8_t)(col + OLED_COL_OFFSET); // bù cột cho SH1106
  uint8_t cmd[3] = {(uint8_t)(0xB0 | (page & 7)), (uint8_t)(c & 0x0F), (uint8_t)(0x10 | (c >> 4))};
  return cmds(cmd, 3);
}

void oled_update_1page(void) {
  if (!s_ok) return;
  uint8_t p = s_next_page;
  s_next_page = (s_next_page + 1) & 7;
  if (!set_pos(p, 0)) return;
  // Transaction data: control 0x40 + 128 byte. Chia 2 nửa để buffer nhỏ.
  uint8_t hdr = 0x40;
  // Nửa 1: gửi control + 64 byte bằng 2 lần write? SSD1306 giữ con trỏ qua transaction
  // nên chia 2 transaction vẫn đúng vị trí (set_pos 1 lần, data nối tiếp).
  uint8_t b[65];
  b[0] = hdr;
  memcpy(&b[1], &s_fb[p][0], 64);
  if (!i2c1_write_buf(s_oaddr, b, 65)) return;
  memcpy(&b[1], &s_fb[p][64], 64);
  i2c1_write_buf(s_oaddr, b, 65);
}

void oled_update_all(void) {
  if (!s_ok) return;
  for (uint8_t p = 0; p < 8; p++) {
    if (!set_pos(p, 0)) return;
    uint8_t b[65];
    b[0] = 0x40;
    memcpy(&b[1], &s_fb[p][0], 64);
    if (!i2c1_write_buf(s_oaddr, b, 65)) return;
    memcpy(&b[1], &s_fb[p][64], 64);
    if (!i2c1_write_buf(s_oaddr, b, 65)) return;
  }
}
