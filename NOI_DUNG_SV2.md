# Nội dung thực hiện của sinh viên 2

## Nhiệm vụ

Sinh viên 2 phụ trách xây dựng driver MPU6500 và khối thu thập dữ liệu cảm biến cho hệ thống ước lượng góc nghiêng. Đầu ra của module là dữ liệu gia tốc, vận tốc góc và thông tin thời gian để sinh viên 3 sử dụng trong các thuật toán ước lượng góc.

Bản mã nguồn hiện tại sử dụng giao tiếp I2C mức thanh ghi trên STM32F103. Kế hoạch ban đầu định hướng SPI; nhóm cần thống nhất giao tiếp trước khi tích hợp chính thức.

## Nội dung đã triển khai trong mã nguồn

1. Xây dựng các hàm đọc, ghi thanh ghi và đọc liên tiếp qua I2C1. Kiểm tra WHO_AM_I để nhận dạng MPU6500, sau đó reset và cấu hình cảm biến.
2. Cấu hình dải đo gia tốc ±4 g, vận tốc góc ±500 °/s, DLPF và tần số lấy mẫu mục tiêu 500 Hz. Đọc lại thanh ghi để phát hiện cấu hình không thành công.
3. Đọc khối 14 byte từ thanh ghi ACCEL_XOUT_H, ghép byte cao/thấp thành số nguyên có dấu 16 bit và tách dữ liệu ba trục accelerometer, ba trục gyroscope.
4. Cấu hình Data Ready và EXTI0 tại PB0. ISR chỉ ghi nhận thời gian và sự kiện; thao tác I2C và xử lý dữ liệu được thực hiện trong vòng lặp chính.
5. Xây dựng hiệu chuẩn bias gyro bằng trung bình mẫu đứng yên, có kiểm tra timeout, lỗi đọc và chuyển động sơ bộ. Xây dựng hàm đo trung bình từng mặt cùng hàm tính offset/scale accelerometer từ sáu tư thế.
6. Chuyển đổi dữ liệu raw sang đơn vị g và °/s. Bổ sung timestamp, dt và cấu trúc SensorData_t để bàn giao cho khối thuật toán.
7. Bổ sung trạng thái lỗi và bộ đếm sự kiện để hỗ trợ kiểm tra dữ liệu và phát hiện mẫu không được bàn giao.

## Nguyên lý xử lý

Khi MPU6500 phát ngắt Data Ready, STM32 ghi nhận sự kiện và timestamp. Vòng lặp chính đọc dữ liệu raw bằng burst read, áp dụng các tham số hiệu chuẩn và chuyển sang đơn vị vật lý. Mẫu hợp lệ được đóng gói vào SensorData_t. Mẫu lỗi không được chuyển sang bộ lọc; khoảng thời gian dt được tính giữa các mẫu bàn giao thành công.

Hiệu chuẩn gyro tính bias trung bình khi cảm biến đứng yên. Hiệu chuẩn accelerometer xác định offset và hệ số scale riêng từng trục từ giá trị trung bình ở hai tư thế +1 g và −1 g. Các tham số cần lấy từ phần cứng thật, không sử dụng số liệu minh họa để công bố kết quả.

## Sản phẩm bàn giao

- main.c: ví dụ tích hợp và kiểm tra module.
- mpu6500_i2c.h: cấu trúc dữ liệu và các hàm giao tiếp.
- mpu6500_i2c.c: mã nguồn driver, acquisition và calibration.
- README_SV2.md: nối dây, cấu hình, API, kiểm tra, giới hạn và hướng dẫn Git.
- tests/: kiểm tra cú pháp mô phỏng và phép tính trên máy tính.

## Trạng thái kiểm chứng

Đã kiểm tra cú pháp C99 với header mô phỏng và các phép tính đổi đơn vị, trừ bias, hiệu chuẩn accel. Chưa build với project Keil hoàn chỉnh hoặc thử trên STM32/MPU6500 thực tế. Do đó chưa có căn cứ để khẳng định đạt lấy mẫu ổn định 500 Hz hoặc đạt một mức sai số cụ thể.

| Nội dung cần đo | Kết quả thực tế |
|---|---|
| WHO_AM_I và cấu hình đọc lại | Chưa đo |
| Tần số lấy mẫu, min/mean/max dt | Chưa đo |
| Số lỗi đọc, sự kiện bị bỏ qua | Chưa đo |
| Gyro đứng yên trước/sau hiệu chuẩn | Chưa đo |
| Accelerometer 6 mặt trước/sau hiệu chuẩn | Chưa đo |
| Build/nạp Keil và chạy liên tục | Chưa thử |

Chỉ cập nhật bảng sau khi có số liệu. Sinh viên 2 cần giải thích được trình tự bus, cấu hình cảm biến, công thức hiệu chuẩn và đơn vị đầu ra khi bàn giao/bảo vệ.
