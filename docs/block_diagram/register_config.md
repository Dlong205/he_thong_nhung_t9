# Cấu hình thanh ghi thực tế — baremetal (2026-10-09)

Bằng chứng đề Khó K03: các ngoại vi trong đề (I2C/EXTI/TIM/USART/GPIO/DWT) đều viết
mức thanh ghi; HAL chỉ dùng cho nền tảng (clock init, NVIC wrapper, HAL_GetTick).

## 1. Clock + GPIO LED (`board.c`)
- `HAL_Init` + `HAL_RCC_OscConfig` (HSE 8MHz) + `HAL_RCC_ClockConfig`:
  PLL x9 → SYSCLK 72MHz, APB1 /2 = 36MHz, APB2 /1 = 72MHz, FLASH_LATENCY_2.
- Gate clock bằng thanh ghi: `RCC->APB1ENR` I2C1EN/TIM2EN; `RCC->APB2ENR` IOPA/B/CEN + AFIOEN + USART1EN.
- LED PC13: `GPIOC->CRH` MODE13=10 (output 2MHz), lật `GPIOC->ODR`.

## 2. I2C1 PB6/PB7 400kHz (`i2c_reg.c`) — 100% thanh ghi
- `GPIOB->CRL`: MODE=11 CNF=11 (AF Open-Drain 50MHz).
- `CR2.FREQ=36` (PCLK1), `CCR = FS | 30` (400kHz DUTY=0), `TRISE=12`.
- `CR1 = PE|ACK`; polling `SB/ADDR/TXE/RXNE/BTF` có timeout 20000 vòng.
- Chống treo (bài học bring-up thật):
  - xóa cờ lỗi `AF/BERR/ARLO/OVR` bằng ghi 0 trước mỗi phiên (F1 clear kiểu này);
  - BUSY kẹt giả → PE=0→1 reset state machine;
  - SDA bị slave giữ → `i2c1_recover()` bit-bang 18 xung SCL + STOP chuẩn;
  - đọc burst 14B theo EV6_3: chờ BTF lần 2 TRƯỚC STOP + khóa ngắt.
- Bằng chứng: boot in `CR1/CR2/CCR/TRISE` + `APB1ENR` ra UART.

## 3. EXTI PA0 cho MPU Data Ready (`exti.c`)
- PA0 input pull-down (`GPIOA->CRL` CNF=10, ODR=0).
- `AFIO->EXTICR1` chọn PA0; `EXTI->RTSR/IMR/PR`; NVIC enable.
- ISR chỉ set flag (ngắn), loop chính xử lý mẫu.
- Đo thực tế: `dt≈2000us`, `exti ok` tăng đều, `timeout=0`.

## 4. TIM2_CH2 PWM servo 50Hz (`servo_ref.c`)
- `GPIOA->CRL` PA1 AF Push-Pull (MODE=11 CNF=10).
- `PSC=72-1` (1MHz), `ARR=20000-1` (20ms = 50Hz), `CCMR1` OC2M=110 PWM mode 1 + preload,
  `CCER` CC2E, `CR1` ARPE + CEN.
- `CCR2 = 1500 + goc*500/90` us. Đã đo bằng thanh ghi: 0°→1500, +45°→1750, -45°→1250us.

## 5. USART1 PA9/PA10 115200, TX ngắt ring (`usart_reg.c`)
- `GPIOA->CRH`: PA9 AF-PP 50MHz, PA10 input pull-up.
- `BRR = PCLK2/baud = 625` (0x271). **Bài học:** công thức chia mantissa/frac ban đầu
  tính sai hệ số 16 → chip phát ~17.8k baud → PC nhận rác; đã sửa và kiểm chứng
  bằng đọc thanh ghi BRR trên mạch. (Ghi vào báo cáo mục "lỗi bring-up thực tế".)
- `CR1 = UE|TE|RE|RXNEIE`; `TXEIE` bật khi ring có dữ liệu.
- TX ring 512B + ISR TXE (non-blocking — không phá nhịp 500Hz);
  RX ring 128B + ISR RXNE cho lệnh `REF/SEQ/MAGCAL/BIN/OLED`.
- Telemetry gói binary `AA 55 | LEN | TS | 14xf32 | EXEC | DT | rkfix | pkfix | ref | yawr | yawt | CRC8`
  (85B @50Hz, đo 156 frame/3s CRC hợp lệ 100%, bad=0).

## 6. DWT đo thời gian thực thi (`timing.c`)
- `DEMCR.TRCENA=1`, `DWT_CTRL.CYCCNTENA=1`, đọc `DWT_CYCCNT` → `exec_us`.
- Đo thực tế: exec ≈ 668us < Ts = 2000us (margin ~3x).

## 7. Những gì vẫn dùng HAL (đúng quy định nền tảng)
- `HAL_Init`, cấu hình clock (`HAL_RCC_*`), NVIC wrapper (`HAL_NVIC_*`), `HAL_GetTick` đếm ms.
- KHÔNG dùng HAL cho ngoại vi ăn điểm: I2C/EXTI/TIM/PWM/USART/GPIO/DWT đều truy cập thanh ghi trực tiếp.
- Không `HAL_Delay` trong vòng lặp chính (chỉ boot/calib).
