// SV1 - board: HAL_Init + SystemClock 72MHz + LED PC13.
// RM0008: RCC_CR HSEON/HSERDY/PLLON/PLLRDY, RCC_CFGR PLLSRC/PLLMUL/PPRE1/PPRE2/SWS,
// FLASH_ACR LATENCY/PRFTBE. Dùng HAL_RCC để đúng chuẩn Cube, vẫn giải thích được
// vì sao peripheral chết nếu chưa bật clock (gate README Phase 1).
#include "board.h"
#include "stm32f1xx_hal.h"

static uint32_t s_sysclk = 72000000;

static void SystemClock_Config(void) {
  RCC_OscInitTypeDef osc = {0};
  osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  osc.HSEState = RCC_HSE_ON;
  osc.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  osc.PLL.PLLState = RCC_PLL_ON;
  osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  osc.PLL.PLLMUL = RCC_PLL_MUL9; // 8MHz x9 = 72MHz
  if (HAL_RCC_OscConfig(&osc) != HAL_OK) while (1);
  RCC_ClkInitTypeDef clk = {0};
  clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  clk.AHBCLKDivider = RCC_SYSCLK_DIV1;   // HCLK 72MHz
  clk.APB1CLKDivider = RCC_HCLK_DIV2;    // PCLK1 36MHz (I2C1/TIM2)
  clk.APB2CLKDivider = RCC_HCLK_DIV1;    // PCLK2 72MHz (GPIO/USART1/AFIO)
  if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK) while (1);
  s_sysclk = HAL_RCC_GetSysClockFreq();
}

void board_init(void) {
  HAL_Init();
  SystemClock_Config();
  // LED PC13: push-pull 2MHz. GPIOC trên APB2.
  // Thanh ghi: RCC_APB2ENR.IOPCEN=1, GPIOC_CRH.MODE13=10/ CNF13=00, ODR13=1 (tắt, LED active-low)
  RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
  GPIOC->CRH &= ~(GPIO_CRH_MODE13 | GPIO_CRH_CNF13);
  GPIOC->CRH |= GPIO_CRH_MODE13_1; // OUTPUT 2MHz
  GPIOC->ODR |= GPIO_ODR_ODR13;
}

uint32_t board_millis(void) { return HAL_GetTick(); }
void board_delay_ms(uint32_t ms) { HAL_Delay(ms); } // chỉ boot, không gọi trong loop
// BẮT BUỘC với HAL: HAL_Init bật ngắt SysTick 1ms, nếu thiếu handler này CPU sẽ
// nhảy vào Default_Handler và kẹt cứng (không bao giờ vào main). Handler chỉ tăng tick.
void SysTick_Handler(void) { HAL_IncTick(); }
void board_led_toggle(void) { GPIOC->ODR ^= GPIO_ODR_ODR13; }
void board_led_on(void) { GPIOC->ODR &= ~GPIO_ODR_ODR13; }
void board_led_off(void) { GPIOC->ODR |= GPIO_ODR_ODR13; }
uint32_t board_sysclk_hz(void) { return s_sysclk; }
uint32_t board_pclk1_hz(void) { return HAL_RCC_GetPCLK1Freq(); }
uint32_t board_pclk2_hz(void) { return HAL_RCC_GetPCLK2Freq(); }
