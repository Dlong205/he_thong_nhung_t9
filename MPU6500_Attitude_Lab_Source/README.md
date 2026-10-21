# MPU6500 Attitude Lab — Source

Source của website đang chạy tại:

<https://mpu6500-attitude-lab.duc01663.chatgpt.site/>

Commit nguồn: `b09d0790303d846fc1e504be4bb39b5d77126627`

## Cấu trúc

- `dist/index.html`: giao diện chính.
- `dist/styles.css`: toàn bộ giao diện và responsive.
- `dist/app.js`: điều khiển giao diện, Web Serial, đồ thị và chỉ số.
- `dist/protocol.js`: giải mã packet UART 77 byte và CRC8.
- `dist/aircraft.js`: mô hình máy bay 3D.
- `dist/wiring.js`: hình/sơ đồ nối dây trên web.
- `.openai/hosting.json`: cấu hình dự án Sites hiện tại.

Giao diện hiện có nút bật/tắt riêng từng đường trên đồ thị Roll/Pitch/Yaw.
Trên đồ thị, dùng `+`/`−` hoặc cuộn chuột để đổi cửa sổ thời gian từ 1 đến
30 giây; bấm **Mở rộng** để xem biểu đồ lớn, kéo ngang để xem dữ liệu cũ và
bấm **Về hiện tại** để tiếp tục theo dõi trực tiếp. Web giữ khoảng 60 giây
dữ liệu gần nhất trong bộ nhớ đồ thị; nút **Xuất CSV** vẫn dùng toàn bộ phiên.
Yaw có các mức làm mượt vòng (0,2/0,6/1,2 s) chỉ áp dụng trên web, giữ
nguyên góc gốc trong gói STM32 và CSV. Chấm đỏ `MPU` giữa thân máy bay là mốc
trực quan của mô hình, không xác nhận vị trí gá cảm biến thật. Sơ đồ chân trên
trang dùng hai bus: MPU ở I2C2 PB10/PB11; OLED và QMC ở I2C1 PB6/PB7.

## Chạy trên máy

Không mở trực tiếp bằng `file://` nếu muốn dùng Web Serial. Trong thư mục source,
mở Command Prompt hoặc PowerShell và chạy:

```bash
python -m http.server 8080 --directory dist
```

Sau đó dùng Chrome, Edge hoặc Firefox 151+ bản desktop mở:

```text
http://localhost:8080
```

Web Serial cần trình duyệt desktop có hỗ trợ và phải đóng các terminal khác đang chiếm cổng COM. Trên Firefox 151+, hãy gỡ extension ``WebSerial for Firefox`` cũ vì extension có thể xung đột với API tích hợp. Bản Firefox Snap trên Ubuntu cũng có thể không nhìn thấy `/dev/ttyUSB*` do sandbox; khi đó dùng Firefox DEB chính thức hoặc Chrome/Edge.

## Giao thức STM32

Website hiện nhận packet nhị phân đúng 77 byte:

```text
AA 55 | LEN=73 | timestamp u32 | 14 x float32 |
exec u16 | dt u16 |
ref int8 | yaw raw float32 | yaw tilt float32 | CRC8
```

Baud mặc định: `115200`.

Các lệnh điều khiển chính: `BIN 1`, `BIN 0`, `REF <góc>`, `SEQ STATIC`,
`SEQ STEP`, `SEQ DRIFT`, `SEQ STOP`, `MAGCAL`, `OLED`.

`REF/SEQ` điều khiển servo trực tiếp bằng PWM 50 Hz trên PA1/TIM2_CH2 của
STM32. Website chỉ cần mở một cổng STM32: vừa nhận telemetry, vừa gửi góc
`-90..+90`. Servo dùng nguồn 5 V đủ dòng và nối GND chung với STM32.

## Chỉnh sửa

Đây là web tĩnh, không cần npm hoặc framework. Chỉnh các file trong `dist/`, tải
lại trang bằng `Ctrl+F5` để xem thay đổi. Việc chỉnh bản local không tự thay đổi
website đang triển khai.
