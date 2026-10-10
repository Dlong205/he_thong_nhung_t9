# EXTI + Jitter 2026-09-28 (USE_EXTI=1, INT->PA0)

## Cau hinh

- MPU `INT_ENABLE.DATA_RDY_EN=1` (xung active-high 50us), STM32 PA0 INPUT_PULLDOWN + attachInterrupt RISING.
- Bang chung thanh ghi in moi boot: AFIO_EXTICR1, EXTI_IMR/RTSR/FTSR (xem `exti_dump()`).
- Vong loop cho co `exti_consume()` + fallback timeout 5ms + dem `int_ok/timeout`.

## Ket qua

- `# exti ok=2250..4500 timeout=0` lien tuc: 100% mau kich boi INT, khong roi vao fallback.
- `dt_us` (500Hz): mean 1998.1us, std 1.84us, min 1993 / max 2005, span 12us.
- Gate jitter PASS (std < 50us). Polling truoc do span ~2us; EXTI span ~12us do ngat that + I2C, van tot.

## Bai hoc

- Nhan RST don thuan co the treo bus I2C (SDA giu thap giua transaction) -> board im lang, USB con nhung khong stream. Fix: power-cycle rut/cam micro-USB 3s.
- Khong cap 2 nguon (ST-Link 3.3V + USB 5V) khi nap.
