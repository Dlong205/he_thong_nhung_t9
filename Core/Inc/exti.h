#pragma once
#include <stdbool.h>
// SV1 - EXTI PA0 cho MPU Data Ready (RISING, xung 50us), THANH GHI THẬT.
// AFIO_EXTICR1.EXTI0=PA, EXTI_IMR/RTSR bit0, NVIC EXTI0_IRQn.
// ISR chỉ set flag, xử lý chính ở loop (không chạy Kalman/printf trong ngắt).
void exti_init_pa0(void);
bool exti_consume(void); // true neu co xung INT tu lan goi truoc
void exti_dump(void);    // AFIO/EXTI ra USART làm bằng chứng
