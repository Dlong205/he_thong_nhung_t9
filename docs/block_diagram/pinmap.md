# Pin map chốt (Blue Pill F103C8 + GY-6500 + OLED 0.96 + QMC5883P + ST-Link + CH340)

Ngày chốt: 2026-10-09. Nền baremetal STM32Cube + thanh ghi (bỏ Arduino/USB-CDC).
Bus I2C1 duy nhất PB6/PB7 cho cả 3 thiết bị. USART1 PA9/PA10 qua CH340 460800.

## 1. Sơ đồ dây (nhìn từ trên Blue Pill, USB-C ở trên)

```text
                     Blue Pill F103C8
                  ┌─────────────────────┐
  ST-Link SWDIO ──┤DIO              5V  ├── VCC servo SG90 (đỏ)
  ST-Link SWCLK ──┤CLK              3V3 ├── VCC MPU + OLED + QMC (KHÔNG lấy 5V cho 3 module này)
  ST-Link GND ────┤GND              GND ├── GND chung (ST-Link + CH340 + 4 module + servo nâu)
  MPU/QMC INT ────┤PA0              PA1 ├── servo signal cam (TIM2_CH2 PWM 50Hz)
  CH340 TXD ──────┤PA10(RX)         PA9 ├── CH340 RXD (cross!) (USART1 460800)
                  │     PB6/SCL ────┼── SCL MPU + SCL OLED + SCL QMC (chung bus)
                  │     PB7/SDA ────┼── SDA MPU + SDA OLED + SDA QMC (chung bus)
  LED onboard ────┤PC13             GND ├── (dự phòng)
                  └─────────────────────┘
```

## 2. Bảng nối dây chi tiết

| Blue Pill | MPU6500 GY-6500 | OLED 0.96 SSD1306 | QMC5883P | Servo SG90 | CH340 | Ghi chú |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| 3.3V | VCC | VCC | VCC | — | — | 3 module I2C ăn 3.3V. QMC **cấm 5V** (2.5-3.6V) |
| 5V | — | — (nếu module ghi 5V thì vẫn cắm 3.3V cho an toàn bus) | — | VCC đỏ | — | Servo ăn 5V riêng, ~200-500mA lúc quay |
| GND | GND | GND | GND | GND nâu | GND | **Bắt buộc chung mass hết** |
| PB6 | SCL | SCL | SCL | — | — | I2C1_SCL, pull-up 4.7k (module đã có sẵn, 3 cái song song ~1.5k vẫn OK 400kHz) |
| PB7 | SDA | SDA | SDA | — | — | I2C1_SDA |
| GND | AD0 | — | — | — | — | AD0=GND → MPU addr 0x68 |
| 3.3V | NCS | — | — | — | — | Kéo high ép MPU mode I2C |
| GND | FSYNC | — | — | — | — | Không dùng |
| PA0 | INT | — | — | — | — | MPU Data Ready RISING, input pull-down |
| PA1 | — | — | — | Signal cam | — | TIM2_CH2 PWM 50Hz 1-2ms |
| PA9 (TX) | — | — | — | — | RXD | USART1 TX → CH340 RX (cross) |
| PA10 (RX) | — | — | — | — | TXD | USART1 RX ← CH340 TX (cross) |
| SWDIO/SWCLK | — | — | — | — | — | Nạp ST-Link, **chỉ 3 dây DIO/CLK/GND, bỏ 3.3V** chống xung nguồn |

## 3. Địa chỉ I2C (quét 1 bus, không con nào trùng)

| Thiết bị | Addr 7-bit | Addr 8-bit W/R | Thanh ghi ID | Giá trị đúng |
| :--- | :--- | :--- | :--- | :--- |
| MPU6500 | 0x68 | 0xD0/0xD1 | WHO_AM_I 0x75 | 0x70 (0x68 là MPU6050 → sai module) |
| OLED SSD1306 | 0x3C | 0x78/0x79 | — (gửi lệnh, không có ID đọc) | init ACK là PASS |
| QMC5883P | 0x2C | 0x58/0x59 | CHIPID 0x00 | 0x80 |

- Tốc độ I2C1 400kHz Fast (`FREQ=36/CCR=30/TRISE=12`, PCLK1 36MHz).
- Thứ tự init trong `main.c`: MPU trước (bắt buộc, fail là treo nháy LED), sau đó OLED + QMC **không bắt buộc** (absent vẫn chạy, báo `# OLED ABSENT / QMC ABSENT`, yaw=0).

## 4. OLED hiển thị gì (4 dòng, refresh ~10Hz)

```text
R+12.3 P-45.6     <- Roll/Pitch Kalman (dòng 0)
Y+178 T+179       <- Yaw raw + tilt (dòng 1, 0 nếu không có mag)
REF+30 STATIC     <- góc servo lệnh + sequence (dòng 2)
E720 D1998 MAG    <- exec_us + dt_us + MAG/NOMAG (dòng 3)
```

## 5. Bài học phần cứng (giữ từ bản cũ + mới)

1. Không cấp 2 nguồn (ST-Link 3.3V + USB 5V) → bỏ dây 3.3V ST-Link.
2. CH340 TX/RX phải cross + chung GND. Baud giờ là **460800** (không phải 115200).
3. 3 module I2C chung bus: nếu 1 con chập SDA là cả bus chết → khi debug mất MPU, rút OLED+QMC ra test từng con.
4. QMC để xa servo + motor (nam châm motor làm lệch yaw vài chục độ). Gá QMC cách servo ≥5cm.
5. OLED ăn dòng ~20mA, lấy 3.3V Blue Pill được. Servo lấy 5V riêng.
