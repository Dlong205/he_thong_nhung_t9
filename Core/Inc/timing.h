#pragma once
#include <stdint.h>
// SV2 - DWT CYCCNT đo exec time chuẩn DWT theo đề (CoreDebug DEMCR.TRACEENA,
// DWT_CTRL.CYCCNTENA). Không dùng HAL_GetTick cho benchmark vì phân giải 1ms.
void timing_init(void);
uint32_t timing_cycles(void);
uint32_t timing_cyc_to_us(uint32_t cyc); // dùng SystemCoreClock, không F_CPU Arduino
uint32_t timing_ms(void);                // DWT-based ms (dự phòng khi HAL chưa init)
