# SV2 – MPU6500 Driver / Data Acquisition

Bản bàn giao dựa trên 3 file được cung cấp: `main.c`, `mpu6500_i2c.c`, `mpu6500_i2c.h`.
Nhánh: `sv2-mpu6500-driver`.

## 1. Phạm vi và trạng thái

SV2 phụ trách nhận dạng, cấu hình MPU6500, đọc raw 6 trục, ngắt Data Ready, hiệu chuẩn và cung cấp `SensorData_t` cho SV3.

**Bản này dùng I2C1 mức thanh ghi**, giữ hướng của code đầu vào. Bản kế hoạch ban đầu dùng SPI; vì vậy đây là bản I2C để thực hành/bàn giao, chưa phải phần tích hợp SPI của kế hoạch. Nhóm cần thống nhất thay đổi giao tiếp; nếu vẫn bắt buộc SPI, phải thay tầng bus và phối hợp API SV1 trước khi tích hợp. Không đổi tên I2C thành SPI trong báo cáo.

Có sẵn mã nguồn, ví dụ chạy và hướng dẫn kiểm tra. Đã kiểm tra cú pháp C99 với header mô phỏng trên máy tính và kiểm tra các phép đổi đơn vị/hiệu chuẩn accel. **Chưa build bằng Keil với device pack thật, chưa nạp mạch, chưa đo được tần số hoặc sai số thực nghiệm.** Đây không phải một project Keil đầy đủ: cần startup, system_stm32f10x.c, CMSIS và cấu hình target từ project hiện có.

## 2. Các file

| File | Vai trò |
|---|---|
| main.c | Ví dụ khởi động, hiệu chuẩn gyro, vòng lặp và EXTI0 ISR |
| mpu6500_i2c.h | Khai báo API, đơn vị, SensorData_t |
| mpu6500_i2c.c | I2C register-level, driver MPU6500, TIM2, hiệu chuẩn, acquisition |
| NOI_DUNG_SV2.md | Nội dung trình bày/báo cáo và tiêu chí bàn giao |
| tests/ | Kiểm tra trên máy tính; không thêm vào project Keil |

Không có Complementary/Kalman hoặc Python trong gói này; phần đó thuộc SV3/SV4 theo phân công.

## 3. Nối dây

| MPU6500 | STM32F103 |
|---|---|
| VCC của module phù hợp 3.3 V | 3.3 V |
| GND | GND |
| SCL | PB6 |
| SDA | PB7 |
| INT | PB0 |
| AD0 | GND, chọn địa chỉ 7 bit 0x68 |
| CS/NCS | 3.3 V để chọn I2C |

SCL/SDA cần pull-up lên **3.3 V**, ví dụ 4.7 kΩ với dây ngắn; kiểm tra module đã có hay chưa và kiểm chứng sườn tín hiệu ở 400 kHz. Không để CS/AD0 trôi. Không đưa mức logic 5 V vào chân MPU.

Địa chỉ I2C 0x68 và ID WHO_AM_I 0x70 là hai giá trị khác nhau. Driver chỉ nhận MPU6500 ID 0x70; không tự coi MPU6050/MPU9250 là cùng loại.

## 4. Yêu cầu Keil

1. Dùng project STM32F103C8T6 hiện có với `stm32f10x.h`, startup đúng chip và `system_stm32f10x.c`. Thường target dùng định nghĩa `STM32F10X_MD`.
2. Startup gọi `SystemInit()` trước `main()`. Cấu hình HCLK=72 MHz, APB1=36 MHz (chia 2); khai báo HSE_VALUE phải đúng thạch anh. Code kiểm tra các điều kiện này và dừng với `app_status=-1` nếu không khớp; không tự đổi clock cả project.
3. Add `main.c`, `mpu6500_i2c.c` vào source group; thêm thư mục header vào Include Paths. Chọn C99 trong Options for Target → C/C++ (Arm Compiler 5: `--c99`).
4. Mỗi project chỉ có một `main()`, một `EXTI0_IRQHandler()` và một `TIM2_IRQHandler()`. Nếu file `stm32f10x_it.c` đã định nghĩa handler, chuyển/ghép nội dung về đó và bỏ định nghĩa trùng.
5. Bản demo dành riêng I2C1, PB6/PB7, EXTI0/PB0 và TIM2. TIM2 interrupt có priority 0, EXTI0 priority 1. Khi ghép nhóm phải thống nhất timer, pin, NVIC priority; không để SV1 cấu hình lại các tài nguyên này sau init.
6. Không cần bật `USE_STDPERIPH_DRIVER` nếu project không sử dụng thư viện SPL; code dùng thanh ghi và CMSIS, không gọi HAL.
7. Build, nạp và Debug. Giữ mạch nằm yên khi khởi động để lấy 500 mẫu gyro. Lỗi hiệu chuẩn không được tự bỏ qua.

## 5. Cấu hình MPU6500

| Thanh ghi | Giá trị | Ý nghĩa dự kiến |
|---|---|---|
| WHO_AM_I 0x75 | đọc 0x70 | Nhận dạng MPU6500 |
| PWR_MGMT_1 0x6B | 0x80 rồi 0x01 | Reset rồi đánh thức/chọn clock |
| PWR_MGMT_2 0x6C | 0x00 | Bật 6 trục |
| SMPLRT_DIV 0x19 | 0x01 | Mục tiêu 1000/(1+1)=500 Hz |
| CONFIG 0x1A | 0x03 | Gyro DLPF 41 Hz |
| GYRO_CONFIG 0x1B | 0x08 | ±500 °/s; 65.5 LSB/(°/s) |
| ACCEL_CONFIG 0x1C | 0x08 | ±4 g; 8192 LSB/g |
| ACCEL_CONFIG2 0x1D | 0x03 | Accel DLPF 41 Hz |
| INT_PIN_CFG 0x37 | 0x00 | Active-high, push-pull, pulse |
| INT_ENABLE 0x38 | 0x01 | Data Ready |

Init đọc lại các thanh ghi đã ghi để kiểm tra. 500 Hz là cấu hình mục tiêu; chỉ được ghi “đã đạt” sau khi đo trên phần cứng.

Burst read lấy 14 byte từ 0x3B: AX, AY, AZ, nhiệt độ, GX, GY, GZ. Ghép mỗi cặp high/low thành `int16_t`; bỏ 2 byte nhiệt độ.

## 6. Những thay đổi so với file ban đầu

- Giới hạn đúng ID MPU6500 thay vì chấp nhận nhiều chip rồi áp chung cấu hình.
- Bổ sung ACCEL_CONFIG2, PWR_MGMT_2, INT_PIN_CFG và đọc lại cấu hình.
- I2C_ReadReg trả status và ghi dữ liệu qua con trỏ: giá trị 0 hợp lệ không bị nhầm thành lỗi.
- Bảo vệ các đoạn ACK/ADDR/STOP nhạy thời gian, xử lý cặp byte cuối cùng liên tiếp.
- Thay delay vòng lặp xấp xỉ bằng TIM2; kiểm tra điều kiện clock.
- Bổ sung timestamp, dt, bộ đếm sự kiện và giao diện SensorData_t.
- Hiệu chuẩn gyro hủy khi lỗi/timeout; chỉ ghi bias sau đủ mẫu hợp lệ. Không còn lấy tổng của ít mẫu chia cho toàn bộ số mẫu yêu cầu.
- Kiểm tra chuyển động sơ bộ bằng độ lớn gia tốc và phương sai gyro. Người dùng vẫn phải giữ mạch đứng yên: quay đều chậm có thể vượt qua kiểm tra này.
- Không dùng 6 số accel minh họa làm thông số đo thật; có hàm đo trung bình một mặt và kiểm tra dữ liệu trước khi cập nhật hiệu chuẩn.

## 7. Hiệu chuẩn accelerometer 6 mặt

Mặc định offset=0, scale=1: dữ liệu được đổi đơn vị theo độ nhạy danh định, **chưa hiệu chuẩn accel**.

Để đo:

1. Trong main.c đặt `SV2_CAPTURE_ACCEL_FACE` từ 0 thành 1, build/nạp lại.
2. Đặt trục +X hướng lên, giữ yên, reset board. Code chờ 2 giây rồi lấy 500 mẫu mới.
3. Khi `app_status=2`, xem `face_mean_raw[0]` và ghi thành `ax_max`. Nếu -4 thì đo thất bại, chỉnh mạch đứng yên rồi reset.
4. Lặp lại -X hướng lên: ghi `face_mean_raw[0]` thành `ax_min`.
5. Với +Y/-Y dùng phần tử [1]; với +Z/-Z dùng phần tử [2].
6. Sáu số là **trung bình raw khi mỗi trục chịu +1 g/-1 g**, không phải cực trị nhiễu trong chuỗi lấy mẫu.
7. Đặt macro về 0. Trong main, sau MPU6500_Init_I2C và trước hiệu chuẩn gyro, thêm lời gọi với đúng 6 giá trị vừa đo:

```c
/* Thay ax_max,... bằng biến/hằng số đo thật của bạn. */
if (!MPU6500_CalibAccel6Face_I2C(&calib_i2c,
        ax_max, ax_min, ay_max, ay_min, az_max, az_min)) {
    app_status = -4;
    while (1) {}
}
```

Đo kiểm lại từng mặt, trục tương ứng nên gần +1/-1 g. Công thức từng trục:

- offset = (mean_plus + mean_minus)/2, đơn vị raw LSB.
- scale = 2×8192/(mean_plus − mean_minus).
- acc_g = (raw − offset)×scale/8192.

Đây là mô hình offset/scale riêng mỗi trục; không sửa sai lệch góc giữa trục. Hiệu chuẩn lưu trong RAM, chưa có lưu Flash. Giữ lại bảng số đo hoặc đưa số thực vào source sau khi kiểm tra.

## 8. Bàn giao cho SV3

```c
typedef struct {
    float ax, ay, az;    /* g */
    float gx, gy, gz;    /* degree/second */
    float dt;           /* second */
    uint32_t timestamp; /* microsecond */
} SensorData_t;
```

Giữ hệ trục theo ký hiệu trên module. SV3 cần thống nhất chiều quay và hướng gá; chưa ánh xạ sang hệ tọa độ xe.

Trong vòng lặp, chỉ xử lý khi `MPU6500_ReadSample_I2C` trả `MPU_SAMPLE_OK`. Với mẫu đầu `dt=0`, dùng accel để khởi tạo bộ lọc; bắt đầu tích phân ở mẫu tiếp theo. Không mặc định mọi dt đều là 0.002.

`timestamp` là thời điểm CPU phục vụ ngắt Data Ready, không phải timestamp phần cứng bên trong MPU. TIM2 đếm 1 µs và mở rộng phần mềm từ bộ đếm 16 bit. Timestamp uint32 tràn sau khoảng 71.6 phút; phép trừ unsigned xử lý qua một lần tràn. Không chặn ngắt quá 65.536 ms vì có thể bỏ lỡ overflow TIM2. Không dùng breakpoint khi đo timing; reset sau khi debug tạm dừng.

Không dùng FIFO: nếu xử lý chậm chỉ giữ sự kiện mới nhất. `g_mpu_dropped` đếm các sự kiện đã quan sát nhưng không bàn giao được; không đảm bảo đếm mọi mẫu mất khi ngắt bị khóa. Nếu có DRDY mới trong lúc đọc, bỏ mẫu đó để tránh gán timestamp cũ cho dữ liệu không chắc chắn. Khi dt tăng lớn, SV3 phải xử lý khoảng trống/reset bộ lọc thay vì giả định dữ liệu đầy đủ.

## 9. Kiểm tra trên mạch

| Biến Watch | Kỳ vọng / cách hiểu |
|---|---|
| app_status | 1: chạy; -1: clock; -2: init/ID/I2C; -3: gyro calibration; 2: đo một mặt xong; -4: đo mặt lỗi |
| g_mpu_id | 0x70 = 112 decimal |
| sample_count | Tăng khi bàn giao mẫu thành công |
| g_mpu_irq_count | Tăng theo ngắt; bao gồm giai đoạn calibration |
| read_errors | Nên không tăng khi vận hành ổn định |
| no_data_fault | 1 khi không có mẫu thành công trong >100 ms |
| sample_dt | Mẫu đầu 0; sau đó gần 0.002 s nếu không bỏ mẫu |
| acc_g_i2c | Khi +Z hướng lên: gần [0,0,+1] g |
| gyro_dps_i2c | Khi đứng yên sau calibration: gần [0,0,0] °/s |
| g_mpu_dropped | Quan sát cùng read_errors để phát hiện xử lý không kịp |

Thử lần lượt: ID → nằm yên → xoay từng trục → calibration → kiểm tra ngắt/timing → rút INT hoặc ngắt kết nối để kiểm tra lỗi. Tắt nguồn trước khi thay đổi dây nối thực hành.

Đo timing bằng logic analyzer trên INT hoặc ghi timestamp qua tầng truyền của nhóm. Với N timestamp liên tục, fs = (N−1)×1e6/(t_last−t_first), xử lý tràn unsigned. Ghi min/mean/max/STD của chênh lệch timestamp và số mẫu lỗi/bỏ. Không suy ra jitter từ vài lần bấm Pause trong Keil.

Bus I2C dùng vòng chờ có giới hạn và reset khi timeout; reset có thể tốn hàng chục ms nên không giữ 500 Hz trong thời gian lỗi. Phục hồi phần mềm không sửa được dây chập, mất pull-up hay slave giữ SCL thấp. Driver chỉ dùng trong main, không reentrant, không gọi bus từ ISR. Chưa triển khai FIFO/DMA hay tự nhận dạng các chip khác.

## 10. Upload lên GitHub theo thư mục hiện tại của bạn

Nhánh sv2 hiện có rồi. Trong Git Bash tại repo `~/Downloads/HTN`:

```bash
git checkout sv2-mpu6500-driver
git status
```

Sau đó chép **nội dung trong thư mục sv2-mpu6500-driver của ZIP** vào repo. Ba file C/H cập nhật thay cho 3 file cùng tên hiện tại. Nếu project đặt source ở Src/Inc thì thay đúng đường dẫn đã có, không tạo bản trùng ở root. Hai file Markdown và tests có thể để ở root repo.

Nếu ba file ở root, chạy:

```bash
git diff --stat
git add main.c mpu6500_i2c.c mpu6500_i2c.h README_SV2.md NOI_DUNG_SV2.md tests/
git diff --cached --stat
git commit -m "feat(sv2): implement MPU6500 acquisition and calibration"
git push -u origin sv2-mpu6500-driver
```

Nếu source ở Src/Inc, thay đường dẫn trong git add cho đúng. Không cần `git init`, thêm remote hoặc `--force` lại. Nếu `nothing to commit`, kiểm tra đã chép bản mới đúng repo và `git diff` có thay đổi chưa; một working tree sạch không tự chứng minh 3 file mong muốn đã nằm trong commit.

Sau push, chọn nhánh `sv2-mpu6500-driver` trên GitHub và kiểm tra nội dung commit. Chưa merge vào nhánh chính cho đến khi nhóm xem phần I2C/SPI và kết quả thử mạch.

## 11. Kiểm tra đã thực hiện

Chạy từ thư mục gói trên máy có Python 3 và GCC:

```bash
python tests/run_host_tests.py
```

Kết quả ở môi trường chuẩn bị gói: PASS. Bao gồm cú pháp C99 với header CMSIS mô phỏng, đổi raw sang g/°/s, dấu âm/dương, hiệu chuẩn 6 mặt bất đối xứng, trừ bias và từ chối dữ liệu hiệu chuẩn sai mà giữ tham số cũ.

Header mô phỏng chỉ phục vụ syntax/test tính toán, không xác nhận địa chỉ/bit thanh ghi thật, trình tự bus, calibration lấy mẫu trên mạch, interrupt hoặc timing. Không thêm tests vào project Keil. Cần build thật và kiểm tra mục 9 trước khi ghi nhận hoàn thành thực nghiệm.

## 12. Tài liệu đối chiếu

- TDK MPU6500: https://www.invensense.tdk.com/en-us/products/motion-sensing/6-axis/mpu-6500
- Register map RM-MPU-6500A-00, Revision 2.1; tìm trong Documents trên trang sản phẩm TDK (đường dẫn PDF cũ có thể chuyển hướng).
- ST RM0008, phần I2C master receiver, GPIO/AFIO/EXTI và general-purpose timer.
- Cấu trúc SensorData_t và phạm vi SV2 lấy từ phần 16 của kế hoạch nhóm đã cung cấp.
