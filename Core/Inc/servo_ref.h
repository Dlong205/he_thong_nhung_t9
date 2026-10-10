#pragma once
// SV3 - Bàn đối chứng: SG90 trên PA1 (TIM2_CH2) PWM 50Hz mức thanh ghi thật.
// TIM2 trên APB1: timer clock 72MHz (x2 khi APB1!=1). PSC=72-1 -> 1MHz,
// ARR=20000-1 -> 20ms (50Hz). CCR2=1000..2000us maps -90..+90°.
// Không HAL_Delay: máy trạng thái board_millis(). Lệnh qua USART: REF/SEQ.
void servoref_init(void);
void servoref_set(float angle_deg);
float servoref_get(void);
void servoref_update(void);
#include <stdbool.h>
bool servoref_parse(const char *line);
const char *servoref_seqname(void);
