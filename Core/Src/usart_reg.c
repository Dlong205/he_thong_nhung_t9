// SV1/SV4 - USART1 register-level. PCLK2=72MHz.
// TX: ring buffer + ngắt TXEIE (không blocking → không phá nhịp sampling 500Hz).
// RX: ring buffer + ngắt RXNEIE cho lệnh REF/SEQ/MAGCAL/OLED.
#include "usart_reg.h"
#include "stm32f1xx_hal.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define RX_BUF 128
static volatile char s_rx[RX_BUF];
static volatile uint16_t s_rx_head = 0, s_rx_tail = 0;
static char s_line[96];
static uint8_t s_line_len = 0;

#define TX_BUF 512
static volatile uint8_t s_tx[TX_BUF];
static volatile uint16_t s_tx_head = 0, s_tx_tail = 0;

void usart1_init(uint32_t baud) {
  // 1. Clock GPIOA + AFIO + USART1 (APB2)
  RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN | RCC_APB2ENR_USART1EN;
  // 2. PA9 TX: Alternate Push-Pull 50MHz (CNF=10 MODE=11)
  GPIOA->CRH &= ~(GPIO_CRH_MODE9 | GPIO_CRH_CNF9);
  GPIOA->CRH |= GPIO_CRH_MODE9 | GPIO_CRH_CNF9_1;
  // 3. PA10 RX: input pull-up (CNF=10 MODE=00, ODR=1)
  GPIOA->CRH &= ~(GPIO_CRH_MODE10 | GPIO_CRH_CNF10);
  GPIOA->CRH |= GPIO_CRH_CNF10_1;
  GPIOA->ODR |= GPIO_ODR_ODR10;
  // 4. BRR: với OVER8=0, giá trị BRR = fPCLK2 / baud (làm tròn).
  //    72MHz/115200 = 625 = 0x271. (Công thức cũ chia sai hệ số 16 -> sai baud!)
  uint32_t pclk2 = HAL_RCC_GetPCLK2Freq();
  USART1->BRR = (uint16_t)((pclk2 + baud / 2u) / baud);
  // 5. CR1: UE + TE + RE + RXNEIE (TXEIE bật khi có dữ liệu trong ring)
  USART1->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE;
  USART1->CR2 = 0;
  USART1->CR3 = 0;
  // 6. NVIC USART1
  HAL_NVIC_SetPriority(USART1_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(USART1_IRQn);
}

// Non-blocking: nhét vào ring, ISR TXE rút dần. Chỉ chờ nếu ring đầy
// (trung bình luôn thấp hơn baud, hiếm khi xảy ra).
void usart1_write_buf(const uint8_t *d, uint16_t len) {
  for (uint16_t i = 0; i < len; i++) {
    uint16_t nh = (s_tx_head + 1) % TX_BUF;
    while (nh == s_tx_tail) { /* ring đầy: đợi ISR rút */ }
    s_tx[s_tx_head] = d[i];
    s_tx_head = nh;
  }
  USART1->CR1 |= USART_CR1_TXEIE;
}

void usart1_putc(char c) {
  usart1_write_buf((const uint8_t *)&c, 1);
}

void usart1_write(const char *s) {
  while (*s) {
    usart1_putc(*s++);
    if (*s == '\n') usart1_putc('\r'); // thêm \r sau \n cho terminal
  }
}

void usart1_printf(const char *fmt, ...) {
  char buf[192];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  usart1_write(buf);
}

bool usart1_getline(char *out, uint32_t out_len) {
  // Nhặt từ ring buffer ISR -> ráp thành dòng \n/\r. Non-blocking.
  while (s_rx_tail != s_rx_head) {
    char c = s_rx[s_rx_tail];
    s_rx_tail = (s_rx_tail + 1) % RX_BUF;
    if (c == '\n' || c == '\r') {
      if (s_line_len > 0) {
        uint32_t n = s_line_len < out_len - 1 ? s_line_len : out_len - 1;
        memcpy(out, s_line, n);
        out[n] = 0;
        s_line_len = 0;
        return true;
      }
    } else if (s_line_len < sizeof(s_line) - 1) {
      s_line[s_line_len++] = c;
    } else {
      s_line_len = 0; // tràn -> bỏ dòng
    }
  }
  return false;
}

void usart1_dump_regs(void) {
  usart1_printf("USART1_CR1=0x%08lX BRR=0x%08lX SR=0x%08lX\r\n",
                (unsigned long)USART1->CR1,
                (unsigned long)USART1->BRR,
                (unsigned long)USART1->SR);
}

// ISR: RX nhét ring lệnh; TX rút ring gửi (TXEIE tự tắt khi hết dữ liệu)
void USART1_IRQHandler(void) {
  uint32_t sr = USART1->SR;
  if (sr & USART_SR_RXNE) {
    char c = (char)(USART1->DR & 0xFF);
    uint16_t nh = (s_rx_head + 1) % RX_BUF;
    if (nh != s_rx_tail) { s_rx[s_rx_head] = c; s_rx_head = nh; }
  }
  if ((sr & USART_SR_TXE) && (USART1->CR1 & USART_CR1_TXEIE)) {
    if (s_tx_tail != s_tx_head) {
      USART1->DR = s_tx[s_tx_tail];
      s_tx_tail = (s_tx_tail + 1) % TX_BUF;
    } else {
      USART1->CR1 &= ~USART_CR1_TXEIE;
    }
  }
}
