// SV2 - DWT register-level (ARMv7-M DWT, có trên cả F1/F4).
#include "timing.h"
#include "stm32f1xx_hal.h"

#define DEMCR   (*((volatile uint32_t*)0xE000EDFC))
#define DWT_CTRL   (*((volatile uint32_t*)0xE0001000))
#define DWT_CYCCNT (*((volatile uint32_t*)0xE0001004))

void timing_init(void) {
  DEMCR |= (1u << 24); // TRCENA
  DWT_CYCCNT = 0;
  DWT_CTRL |= 1u;      // CYCCNTENA
}
uint32_t timing_cycles(void) { return DWT_CYCCNT; }
uint32_t timing_cyc_to_us(uint32_t cyc) {
  uint32_t mhz = HAL_RCC_GetSysClockFreq() / 1000000u; // 72
  if (mhz == 0) mhz = 72;
  return cyc / mhz;
}
uint32_t timing_ms(void) { return timing_cycles() / (HAL_RCC_GetSysClockFreq() / 1000u); }
