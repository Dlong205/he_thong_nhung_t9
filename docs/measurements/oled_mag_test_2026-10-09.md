# Test OLED 0.96 + QMC5883P/Yaw (2026-10-09, FW baremetal 460800)

## 0. Chuẩn bị

- Nạp: `pio run -t upload` (ST-Link, chỉ SWDIO/SWCLK/GND).
- Mở log: `pio device monitor -p /dev/ttyUSB0 -b 460800` hoặc
  `python3 pc/serial_logger.py --port /dev/ttyUSB0 --baud 460800 --out /tmp/t.csv --sec 30`.
- Lệnh gõ trong Monitor (baud 460800, NL): `REF`, `SEQ`, `MAGCAL`, `OLED`.

## 1. Test bus I2C (2 phút, không cần xoay gì)

Mở monitor sau reset, phải thấy theo thứ tự:

```text
# K03 baremetal Cube+REG+OLED+MAG boot
# WHO_AM_I=0x70
# OLED OK 0x3C            <- nếu ABSENT: kiểm tra VCC/GND/SCL/SDA OLED, addr 0x3C
# QMC5883P OK 0x2C        <- nếu ABSENT: kiểm tra VCC 3.3V (cấm 5V), SDA/SCL
# RCC_APB1ENR=... I2C1_CR1=... CR2=... CCR=... TRISE=...   <- bằng chứng thanh ghi
# ... EXTI ...            <- AFIO/EXTI
ts,axg,...ref,yawr,yawt   <- header CSV có yawr/yawt cuối
```

- OLED sáng, dòng 0 hiện `K03 BOOT OK`. Tối om → kiểm tra contrast/VCC, thử rút QMC ra (bus chập).
- `OLED` + Enter → báo `present=1/0`. `MAGCAL` khi không có mag → báo `khong co mag`.

## 2. Test OLED (1 phút)

- Để yên 10s: dòng 0 `R/P` phải đứng gần `R+0.0 P+0.0` (lệch <2° do mặt bàn), dòng 3 `E~700 D~2000`.
- Gõ `SEQ STEP`: dòng 2 `REF` nhảy `0→30→60→0`, dòng 0 Roll bám theo nếu board đang gắn trên servo.
- Nếu chữ rác/dòng đè nhau: bình thường ở bản này (font 5x7, xóa cả dòng trước khi ghi). Chữ mất hẳn 1 page → bus I2C nghẽn (OLED 3ms + telemetry 3.5ms), xem `D` có vọt >8000 không.

## 3. Test QMC/Yaw tĩnh (3 phút)

1. Để board nằm ngang, xa sắt/motor ≥20cm. Đọc telemetry `yawr/yawt` hoặc dòng OLED `Y/T`.
2. Xoay cả cụm **chậm** 360° trên mặt bàn (la bàn): `yawt` phải quét đủ `-180→+180` liên tục, không nhảy cóc. `yawr` (không bù) cũng quét nhưng sẽ sai khi nghiêng — đó là lý do cần bù.
3. Giữ yên 10s: yaw đứng, std <2°. Nhảy >5° → nhiễu nguồn/motor gần đó.

## 4. Calib hard-iron MAGCAL (bắt buộc trước khi chấm Yaw, 1 phút)

Từ trường nhà/xưởng lệch 5-20° nếu không calib:

1. Gõ `MAGCAL` → `# MAGCAL start: xoay ngang 360d/15s`.
2. Trong 15s: xoay board 2 vòng ngang + nghiêng úp/ngửa 1 lần (vẽ số 8 nằm).
3. Hết 15s: `# MAGCAL OK ox=.. oy=.. oz=.. LSB`. `FAIL` = xoay chưa đủ biên (>500 LSB) → làm lại, xoay rộng hơn.
4. Kiểm tra: xoay lại 360°, yaw sai số còn ~2-3° (trước calib có thể 10-20°).

## 5. Test Yaw bù nghiêng (chứng minh tilt-compensation, 2 phút)

1. Hướng yaw về 1 mốc (ví dụ cửa sổ), ghi `yawt`.
2. Giữ nguyên hướng, nghiêng board Roll ±30°: `yawr` trôi 10-20°, `yawt` phải đứng yên ±3°. Đó là điểm ăn tiền phần mở rộng.
3. Log 30s vừa nghiêng vừa xoay nhẹ → `pc/eval_ref.py` bỏ qua yaw (chỉ chấm Roll), xem `yawr/yawt` bằng mắt hoặc vẽ thêm.

## 6. Test tải bus (đảm bảo OLED+MAG không phá 500Hz)

Log 30s để yên, chạy `python3 pc/eval_ref.py /tmp/t.csv`:

- `dt_us mean ~2000 std <50, max-min <200` là PASS. Thỉnh thoảng 1 mẫu `dt ~8000` (OLED page + telemetry cùng tick) chấp nhận được, ghi vào báo cáo.
- `exec_us` tăng từ ~760 (bản chưa OLED) lên ~1200-2000 là bình thường (thêm mag 6B + printf yaw). Vượt 2000 liên tục → giảm OLED xuống (sửa `*5` thành `*10` trong main.c) hoặc tăng baud.
- `# exti ok` tăng đều, `timeout` ~0. Timeout tăng khi OLED block I2C → bus cần kiểm tra pull-up.

## 7. Lỗi hay gặp

| Hiện tượng | Nguyên nhân | Fix |
| :--- | :--- | :--- |
| `# FAIL mpu` nháy LED | MPU mất bus (OLED/QMC chập SDA) | Rút OLED+QMC, test MPU một mình |
| OLED tối, MPU vẫn chạy | OLED addr 0x3D (jumper) / VCC 5V yếu | Đo jumper SA0, cấp 3.3V đủ 20mA |
| Yaw quay 1 vòng báo 2 vòng / ngược | Trục mag ngược board | Đổi dấu `yaw_raw(-my→+my)` trong `yaw.c`, ghi vào báo cáo |
| Yaw lệch 90° cố định | Mount QMC xoay 90° so với MPU | Cộng offset mount vào `yaw_tilt`, đo 1 lần |
| MAGCAL FAIL liên tục | Gần sắt/servo, biên <500 | Ra chỗ thoáng, cách servo 5cm, xoay rộng |
