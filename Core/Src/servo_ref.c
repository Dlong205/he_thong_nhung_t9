// SV3 - Servo reference rig, TIM2_CH2 PA1 register-level (RM0008 §15 TIM).
#include "servo_ref.h"
#include "stm32f1xx_hal.h"
#include "board.h"
#include "usart_reg.h"
#include <string.h>
#include <stdlib.h>

static float s_ref = 0.0f;
static int s_seq = 0; // 0=MANUAL,1=STATIC,2=STEP,3=DRIFT
static uint32_t s_seq_t0 = 0;
static int s_seq_idx = 0;
static const char *s_seq_name = "MANUAL";

static const float SEQ_STATIC[] = {-60,-45,-30,0,30,45,60};
static const uint32_t SEQ_STATIC_HOLD = 10000;
static const float SEQ_STEP[] = {0,30,60,0};
static const uint32_t SEQ_STEP_HOLD = 5000;
static const uint32_t SEQ_DRIFT_HOLD = 330000;

static float clampf(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static void pwm_set_us(uint32_t us) {
  if (us < 500) us = 500;
  if (us > 2500) us = 2500;
  TIM2->CCR2 = us; // 1 tick = 1us (PSC 72-1)
}

void servoref_init(void) {
  // 1. Clock GPIOA (APB2) + TIM2 (APB1)
  RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN;
  RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
  // 2. PA1 AF push-pull 50MHz (TIM2_CH2): CRL MODE1=11 CNF1=10
  GPIOA->CRL &= ~(GPIO_CRL_MODE1 | GPIO_CRL_CNF1);
  GPIOA->CRL |= GPIO_CRL_MODE1 | GPIO_CRL_CNF1_1;
  // 3. TIM2: PSC=72-1 (72MHz->1MHz), ARR=20000-1 (20ms)
  TIM2->PSC = 72 - 1;
  TIM2->ARR = 20000 - 1;
  // CCMR1 CH2 PWM mode 1 + preload: OC2M=110, OC2PE=1
  TIM2->CCMR1 &= ~TIM_CCMR1_OC2M;
  TIM2->CCMR1 |= (6u << TIM_CCMR1_OC2M_Pos) | TIM_CCMR1_OC2PE;
  TIM2->CCER |= TIM_CCER_CC2E;   // enable CH2
  TIM2->CR1 |= TIM_CR1_ARPE;
  TIM2->EGR |= TIM_EGR_UG;       // nạp PSC/ARR
  TIM2->CR1 |= TIM_CR1_CEN;
  servoref_set(0.0f);
  usart1_write("# servoref: SG90 PA1 TIM2_CH2 50Hz, cmd REF <deg> | SEQ STATIC|STEP|DRIFT|STOP\r\n");
}

void servoref_set(float angle_deg) {
  s_ref = clampf(angle_deg, -90.0f, 90.0f);
  // 1500us giữa + 500/90 mỗi độ
  pwm_set_us((uint32_t)(1500.0f + s_ref * (500.0f / 90.0f)));
}

float servoref_get(void) { return s_ref; }
const char *servoref_seqname(void) { return s_seq_name; }

static void seq_goto(float ang, const char *name) {
  servoref_set(ang);
  usart1_printf("# SEQ %s -> ref=%.0f\r\n", name, ang);
}

void servoref_update(void) {
  if (s_seq == 0) return;
  uint32_t now = board_millis();
  if (s_seq == 1) {
    const int N = (int)(sizeof(SEQ_STATIC)/sizeof(SEQ_STATIC[0]));
    if (now - s_seq_t0 >= SEQ_STATIC_HOLD) {
      if (++s_seq_idx >= N) { s_seq = 0; s_seq_name = "MANUAL"; usart1_write("# SEQ STATIC done\r\n"); return; }
      s_seq_t0 = now; seq_goto(SEQ_STATIC[s_seq_idx], "STATIC");
    }
  } else if (s_seq == 2) {
    const int N = (int)(sizeof(SEQ_STEP)/sizeof(SEQ_STEP[0]));
    if (now - s_seq_t0 >= SEQ_STEP_HOLD) {
      if (++s_seq_idx >= N) { s_seq = 0; s_seq_name = "MANUAL"; usart1_write("# SEQ STEP done\r\n"); return; }
      s_seq_t0 = now; seq_goto(SEQ_STEP[s_seq_idx], "STEP");
    }
  } else if (s_seq == 3) {
    if (now - s_seq_t0 >= SEQ_DRIFT_HOLD) {
      s_seq = 0; s_seq_name = "MANUAL"; usart1_write("# SEQ DRIFT done (330s)\r\n");
    }
  }
}

bool servoref_parse(const char *line) {
  if (!line) return false;
  if (strncmp(line, "REF", 3) == 0) {
    s_seq = 0; s_seq_name = "MANUAL";
    servoref_set((float)atof(line + 3));
    return true;
  }
  if (strncmp(line, "SEQ", 3) == 0) {
    const char *p = line + 3;
    while (*p == ' ') p++;
    if (strncmp(p, "STATIC", 6) == 0) {
      s_seq = 1; s_seq_idx = 0; s_seq_t0 = board_millis(); s_seq_name = "STATIC";
      seq_goto(SEQ_STATIC[0], "STATIC");
    } else if (strncmp(p, "STEP", 4) == 0) {
      s_seq = 2; s_seq_idx = 0; s_seq_t0 = board_millis(); s_seq_name = "STEP";
      seq_goto(SEQ_STEP[0], "STEP");
    } else if (strncmp(p, "DRIFT", 5) == 0) {
      s_seq = 3; s_seq_t0 = board_millis(); s_seq_name = "DRIFT";
      servoref_set(0.0f);
      usart1_write("# SEQ DRIFT start: hold 0 deg 330s\r\n");
    } else { s_seq = 0; s_seq_name = "MANUAL"; usart1_write("# SEQ stop\r\n"); }
    return true;
  }
  return false;
}
