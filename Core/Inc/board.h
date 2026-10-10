#pragma once
// SV1 - board: clock + LED + millis. HAL cho phần không chấm điểm,
// thanh ghi giải thích rõ cho phần chấm điểm (xem từng driver).
// Blue Pill F103C8: HSE 8MHz -> PLL x9 -> SYSCLK 72MHz, APB1/2, Flash latency 2.
#include <stdint.h>
void board_init(void);        // HAL_Init + SystemClock 72MHz + LED PC13 + DWT
uint32_t board_millis(void);  // HAL_GetTick (SysTick 1ms)
void board_delay_ms(uint32_t ms); // chỉ dùng ở boot (calib), CẤM trong loop chính
void board_led_toggle(void);
void board_led_on(void);
void board_led_off(void);
uint32_t board_sysclk_hz(void); // 72000000
uint32_t board_pclk1_hz(void);  // 36000000
uint32_t board_pclk2_hz(void);  // 72000000
