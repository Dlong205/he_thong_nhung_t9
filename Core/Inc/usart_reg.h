#pragma once
// SV1/SV4 - USART1 PA9(TX)/PA10(RX) 230400 8N1, mức thanh ghi thật (RM0008 §27).
// RCC_APB2ENR: IOPAEN/AFIOEN/USART1EN. GPIOA_CRH: PA9 AF-PP 50MHz, PA10 input pull-up.
// BRR từ PCLK2. TX: polling TXE. RX: ngắt RXNE + ring buffer cho lệnh REF/SEQ.
#include <stdint.h>
#include <stdbool.h>
void usart1_init(uint32_t baud);
void usart1_putc(char c);
void usart1_write(const char *s);
// Non-blocking: nhét N byte vào ring TX, ISR gửi dần (không chặn loop 500Hz)
void usart1_write_buf(const uint8_t *d, uint16_t len);
void usart1_printf(const char *fmt, ...);
// RX non-blocking cho main: gọi mỗi loop để nhặt lệnh 1 dòng
bool usart1_getline(char *out, uint32_t out_len);
void usart1_dump_regs(void); // in CR1/BRR/SR làm bằng chứng báo cáo
