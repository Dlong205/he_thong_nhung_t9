# Đối chiếu Đề Khó K03 sau chuyển baremetal (2026-10-09)

## Nền: PASS
- `framework=stm32cube` (HAL CubeF1) + Blue Pill F103C8, nạp ST-Link. Bỏ Arduino.
- `board.c`: HAL_Init + PLL 8MHz x9 = 72MHz (PCLK1 36 / PCLK2 72), LED PC13 thanh ghi.
- Quy định 6: không `HAL_Delay` trong loop chính (chỉ boot/calib). Quy định 5: Git module.

## Thanh ghi thật (không còn Wire/attachInterrupt/Servo): PASS code, chờ đo lại trên mạch
- `i2c_reg.c`: I2C1 PB6/PB7 400kHz (FREQ=36/CCR=30/TRISE=12), START/ADDR/TXE/BTF/RXNE/POS/NACK/STOP + timeout. MPU đọc qua driver này.
- `mpu6500.c`: PWR 0x6B/ DIV 0x19 / CFG 0x1A / GCFG / ACFG + burst 14B từ 0x3B + WHO_AM_I 0x70.
- `exti.c`: PA0 pull-down + AFIO_EXTICR1 + IMR/RTSR + NVIC EXTI0, ISR chỉ set flag.
- `timing.c`: DWT DEMCR/DWT_CTRL/CYCCNT, đổi cycle->us bằng SystemCoreClock.
- `usart_reg.c`: USART1 PA9/PA10 230400 (BRR từ PCLK2), TX polling + RX ngắt ring cho REF/SEQ.
- `servo_ref.c`: TIM2_CH2 PA1 PWM 50Hz (PSC=72-1/ARR=20000-1/CCR2=1000..2000), máy trạng thái STATIC/STEP/DRIFT.
- Boot in `RCC_APB1ENR/I2C_CR1-CR2-CCR-TRISE + USART_CR1/BRR + AFIO/EXTI` ra USART làm bằng chứng báo cáo.

## Thuật toán tự viết: PASS (giữ nguyên, port sang C)
- `attitude/complementary/kalman(+fix)`: Acc atan2, gyro int, CF α=0.98, Kalman 2-state Q=0.001/0.003 R=0.03 + Q16.16. Không DMP/lib.

## Số liệu build baremetal (chưa nạp)
- Flash 43.9% (28.7KB/64KB), RAM 4.9% (trước Arduino: 92.2%/27.9%). Driver lớn nhất `servo 1.1KB + i2c 0.9KB`, Kalman 0.3KB → thuật toán nhẹ, I2C burst mới là tải chính.
- `Tproc` kỳ vọng vẫn <2ms (cần đo lại DWT trên mạch vì bỏ USB-CDC, đổi sang UART).

## Còn phải đo lại trên mạch (giữ nguyên số cũ để tham chiếu, không dùng để bảo vệ vội)
1. Nạp ST-Link (chỉ SWDIO/SWCLK/GND), cắm CH340 RX->PA9 TX->PA10 + GND, mở 230400.
2. Kiểm tra `WHO_AM_I=0x70`, `i2c_dump`, `usart_dump`, `exti_dump`.
3. `SEQ STATIC/STEP/DRIFT` + `serial_logger --port /dev/ttyUSB0 --baud 230400` → `eval_ref.py`.
4. Chưa có: OLED/LED7 (SV4), benchmark tách khối từng hàm, báo cáo/video.
