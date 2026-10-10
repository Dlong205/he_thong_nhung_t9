// SV1 - EXTI0 PA0 register-level (RM0008 §9/AFIO + §10/EXTI + NVIC).
#include "exti.h"
#include "stm32f1xx_hal.h"
#include "usart_reg.h"

static volatile bool s_flag = false;

void exti_init_pa0(void) {
  // 1. Clock GPIOA + AFIO
  RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN;
  // 2. PA0 input pull-down: CRL MODE0=00 CNF0=10, ODR0=0
  GPIOA->CRL &= ~(GPIO_CRL_MODE0 | GPIO_CRL_CNF0);
  GPIOA->CRL |= GPIO_CRL_CNF0_1;
  GPIOA->ODR &= ~GPIO_ODR_ODR0;
  // 3. AFIO EXTICR1: EXTI0 <- PA (0000)
  AFIO->EXTICR[0] &= ~AFIO_EXTICR1_EXTI0;
  // 4. EXTI: RISING, unmask, xóa cờ cũ
  EXTI->FTSR &= ~EXTI_FTSR_TR0;
  EXTI->RTSR |= EXTI_RTSR_TR0;
  EXTI->IMR |= EXTI_IMR_MR0;
  EXTI->PR = EXTI_PR_PR0;
  // 5. NVIC
  HAL_NVIC_SetPriority(EXTI0_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);
}

bool exti_consume(void) {
  __disable_irq();
  bool f = s_flag; s_flag = false;
  __enable_irq();
  return f;
}

void exti_dump(void) {
  usart1_printf("AFIO_EXTICR1=0x%08lX EXTI_IMR=0x%08lX RTSR=0x%08lX FTSR=0x%08lX\r\n",
                (unsigned long)AFIO->EXTICR[0], (unsigned long)EXTI->IMR,
                (unsigned long)EXTI->RTSR, (unsigned long)EXTI->FTSR);
}

void EXTI0_IRQHandler(void) {
  if (EXTI->PR & EXTI_PR_PR0) {
    EXTI->PR = EXTI_PR_PR0; // xóa cờ (ghi 1)
    s_flag = true;
  }
}
