# KẾ HOẠCH CHI TIẾT DỰ ÁN

## Ước lượng góc nghiêng bằng MPU6500 -- Complementary Filter và Kalman Filter tự viết trên STM32

> **Phiên bản định hướng:** STM32F103 + MPU6500 + SPI mức thanh ghi + Data Ready Interrupt + Complementary/Kalman + Python realtime/3D  
> **Mục tiêu:** bám yêu cầu đề tài mức Khó, có phần lập trình thanh ghi rõ ràng, thuật toán tự viết, đo đạc định lượng và demo trực quan.

---

## 1. Mục tiêu dự án

Xây dựng một hệ thống nhúng có khả năng:

1. Đọc dữ liệu gia tốc và vận tốc góc 6 trục từ MPU6500.
2. Giao tiếp MPU6500 với STM32F103 bằng SPI.
3. Tự xây dựng các phần driver mức thanh ghi thay vì phụ thuộc hoàn toàn vào HAL.
4. Lấy mẫu ổn định, mục tiêu ban đầu 500 Hz.
5. Hiệu chuẩn accelerometer và gyroscope.
6. Ước lượng Roll/Pitch bằng:
   - Accelerometer.
   - Tích phân Gyroscope.
   - Complementary Filter.
   - Kalman Filter 2 trạng thái tự viết.
7. Có phiên bản `float`; nếu tiến độ cho phép, triển khai thêm fixed-point để so sánh.
8. Đo thời gian thực thi và đánh giá sai số/drift/đáp ứng.
9. Truyền kết quả tới PC.
10. Python hiển thị:
    - Đồ thị realtime.
    - So sánh các phương pháp.
    - Mô hình 3D bám Roll/Pitch.
    - Ghi dữ liệu phục vụ đánh giá.

---

## 2. Kiến trúc tổng thể

```text
       Chuyển động thực tế
               │
               ▼
        ┌─────────────┐
        │   MPU6500   │
        │ Acc + Gyro  │
        └──────┬──────┘
               │
       SPI + INT/Data Ready
               │
               ▼
┌─────────────────────────────────────────────┐
│                  STM32F103                  │
│                                             │
│ RCC / GPIO / SPI / EXTI / TIM / USART      │
│        ↑ phần register-level                │
│                    │                        │
│             MPU6500 Driver                  │
│                    │                        │
│        Ax Ay Az / Gx Gy Gz                  │
│                    │                        │
│               Calibration                   │
│                    │                        │
│       ┌────────────┼────────────┐           │
│       ▼            ▼            ▼           │
│ Acc Angle    Gyro Integral   Fusion          │
│                            ┌────┴────┐       │
│                            ▼         ▼       │
│                      Complementary  Kalman   │
│                            │         │       │
│                            └────┬────┘       │
│                                 │            │
│                          Output Packet       │
└─────────────────────────────────┼────────────┘
                                  │
                              UART/USB
                                  │
                                  ▼
                          ┌──────────────┐
                          │    Python    │
                          ├──────────────┤
                          │ Realtime Plot│
                          │ 3D Visualizer│
                          │ CSV Logger   │
                          │ Evaluation   │
                          └──────────────┘
```

---

## 3. Phạm vi lập trình mức thanh ghi

### 3.1. RCC
Tự cấu hình clock cho các peripheral cần thiết:
- GPIOA/GPIOB/AFIO.
- SPI1.
- USART.
- Timer.
- Các peripheral khác nếu phát sinh.

Mục tiêu cần hiểu:
```text
System Clock
    │
    ├── AHB
    ├── APB1
    └── APB2
          ├── GPIO
          ├── SPI1
          └── USART1
```
Không chỉ viết được code mà phải giải thích được **vì sao peripheral không hoạt động nếu chưa cấp clock**.

### 3.2. GPIO
Tự cấu hình:
- SPI SCK.
- SPI MOSI.
- SPI MISO.
- CS/NCS.
- MPU6500 INT.
- UART TX/RX nếu sử dụng UART.

Cần hiểu các trường `MODE` và `CNF` của STM32F1 và sự khác nhau giữa:
- Input.
- Output Push-Pull.
- Alternate Function Push-Pull.
- Floating/Input Pull-up/Pull-down.

### 3.3. SPI
Đây là một trong các phần register-level chính.

Tự cấu hình:
- Master mode.
- Baud-rate prescaler.
- CPOL/CPHA phù hợp MPU6500.
- MSB first.
- Software/Hardware NSS phù hợp thiết kế.
- Enable SPI.

Tự xử lý:
- `TXE`.
- `RXNE`.
- `BSY`.
- `DR`.
- CS.

API mục tiêu:
```c
void SPI1_Init(void);
uint8_t SPI1_Transfer(uint8_t data);
void SPI1_Transmit(const uint8_t *data, uint16_t len);
```

### 3.4. EXTI + NVIC
Dùng chân `INT` của MPU6500.

Luồng mong muốn:
```text
MPU6500 sample ready
        │
        ▼
       INT
        │
        ▼
STM32 EXTI interrupt
        │
        ▼
sample_ready = 1
        │
        ▼
Main/task xử lý sample
```

ISR phải ngắn. Không chạy toàn bộ Kalman, printf hay truyền packet dài trực tiếp trong ISR nếu không cần thiết.

### 3.5. Timer / bộ đếm thời gian
Timer dùng cho:
- Timebase.
- Kiểm chứng chu kỳ lấy mẫu.
- Đo `dt` nếu cần.
- Benchmark thời gian thực thi thuật toán.

Mục tiêu đo:
```text
Accelerometer calculation : ... us
Gyro integration          : ... us
Complementary             : ... us
Kalman float              : ... us
Kalman fixed-point        : ... us
Total pipeline            : ... us
```

### 3.6. USART
Nếu PC/Python nhận dữ liệu qua USB-UART, tự viết USART register-level là một phần đóng góp tốt.

API tối thiểu:
```c
void USART_Init(void);
void USART_SendByte(uint8_t data);
void USART_SendBuffer(const uint8_t *data, uint16_t len);
```

Không nên để `printf()` blocking làm hỏng sampling 500 Hz. Sau khi hệ thống cơ bản chạy ổn có thể cân nhắc ring buffer/interrupt/DMA.

---

## 4. MPU6500 Driver

### 4.1. Milestone đầu tiên: WHO_AM_I
Trước khi đọc cảm biến:
```text
STM32
 │
 ├── CS LOW
 ├── gửi địa chỉ WHO_AM_I | READ
 ├── nhận response
 └── CS HIGH
```
**Gate:** Không chuyển sang thuật toán cho đến khi đọc ID ổn định và SPI được kiểm chứng.

### 4.2. Các chức năng driver
Dự kiến:
```c
bool MPU6500_Init(void);

uint8_t MPU6500_ReadReg(uint8_t reg);
void MPU6500_WriteReg(uint8_t reg, uint8_t value);

void MPU6500_ReadBurst(
    uint8_t start_reg,
    uint8_t *buffer,
    uint16_t length
);

void MPU6500_ReadRaw(...);
```

### 4.3. Cấu hình cảm biến
Cần chủ động cấu hình và ghi lại trong báo cáo:
- Clock source.
- Sample-rate divider.
- DLPF.
- Accelerometer full-scale.
- Gyroscope full-scale.
- Data Ready Interrupt.

Không dùng giá trị "copy từ mạng" mà không giải thích.

### 4.4. Burst Read
Ưu tiên đọc liên tục block:
```text
ACCEL_XOUT_H
...
ACCEL_ZOUT_L
TEMP_OUT_H/L
GYRO_XOUT_H
...
GYRO_ZOUT_L
```
Sau đó ghép byte thành `int16_t`.

---

## 5. Tổ chức firmware

Gợi ý cấu trúc:
```text
Core/
├── Inc/
│   ├── board.h
│   ├── spi_reg.h
│   ├── usart_reg.h
│   ├── timer_reg.h
│   ├── exti_reg.h
│   ├── mpu6500.h
│   ├── calibration.h
│   ├── attitude.h
│   ├── complementary.h
│   ├── kalman.h
│   └── telemetry.h
│
└── Src/
    ├── main.c
    ├── spi_reg.c
    ├── usart_reg.c
    ├── timer_reg.c
    ├── exti_reg.c
    ├── mpu6500.c
    ├── calibration.c
    ├── attitude.c
    ├── complementary.c
    ├── kalman.c
    └── telemetry.c

pc/
├── serial_receiver.py
├── realtime_plot.py
├── visualizer_3d.py
└── analysis.py

docs/
├── block_diagram/
├── measurements/
└── report/
```

Không bắt buộc giữ chính xác cấu trúc này; mục tiêu là tách **low-level driver / device driver / algorithm / application**.

---

## 6. Sampling 500 Hz

Mục tiêu:
$$f_s = 500 \text{ Hz}$$
$$T_s = \frac{1}{500} = 2 \text{ ms}$$

Không sử dụng kiểu:
```c
while (1)
{
    ReadSensor();
    Filter();
    HAL_Delay(2);
}
```

Ưu tiên event-driven:
```c
while (1)
{
    if (sample_ready)
    {
        sample_ready = 0;

        MPU6500_ReadRaw();
        ProcessSample();
    }

    /* background tasks */
}
```
Phải đo thực tế jitter/chu kỳ sampling thay vì chỉ tuyên bố 500 Hz.

---

## 7. Calibration

### 7.1. Gyroscope bias
Khi cảm biến đứng yên:
$$b_g = \frac{1}{N}\sum_{i=1}^{N}g_i$$
Sau đó:
$$g_{corrected} = g_{measured} - b_g$$
Thực hiện cho X/Y/Z.

### 7.2. Accelerometer
Mục tiêu tốt là calibration 6 mặt:
```text
+X
-X
+Y
-Y
+Z
-Z
```
Ước lượng:
- Offset.
- Scale.

Sau calibration cần lưu kết quả để so sánh **trước/sau calibration**.

---

## 8. Các phương pháp ước lượng góc

### 8.1. Accelerometer-only
Ví dụ Roll/Pitch từ vector trọng lực:
$$Roll_{acc} = \operatorname{atan2}(a_y, a_z)$$
Một dạng Pitch:
$$Pitch_{acc} = \operatorname{atan2}\left(-a_x, \sqrt{a_y^2 + a_z^2}\right)$$
Cần thống nhất hệ tọa độ và quy ước dấu ngay từ đầu.

### 8.2. Gyroscope integration
$$\theta_k = \theta_{k-1} + \omega_k \Delta t$$
Mục tiêu thí nghiệm: chứng minh gyro mượt trong ngắn hạn nhưng bị drift do bias.

### 8.3. Complementary Filter
$$\theta_k = \alpha (\theta_{k-1} + \omega_k \Delta t) + (1-\alpha)\theta_{acc}$$
Không chọn $\alpha$ tùy tiện rồi kết luận. Thử một số cấu hình và giải thích ảnh hưởng.

### 8.4. Kalman 2-state
State:
$$x = \begin{bmatrix} \theta \\ b \end{bmatrix}$$
Trong đó:
- $\theta$: góc.
- $b$: gyro bias.

Input:
$$u = \omega_{gyro}$$

Measurement:
$$z = \theta_{acc}$$

Các bước:
1. Predict state.
2. Predict covariance.
3. Innovation.
4. Innovation covariance.
5. Kalman gain.
6. State update.
7. Covariance update.

API:
```c
typedef struct
{
    float angle;
    float bias;

    float P00;
    float P01;
    float P10;
    float P11;

    float Q_angle;
    float Q_bias;
    float R_measure;
} Kalman_t;

float Kalman_Update(
    Kalman_t *kf,
    float acc_angle,
    float gyro_rate,
    float dt
);
```
**Không dùng thư viện Kalman có sẵn.**

---

## 9. Float và Fixed-point

### Phase A
Hoàn thiện và kiểm chứng `float` trước.

### Phase B
Nếu float đã đúng, xây fixed-point.

Ví dụ nghiên cứu:
- Q15.
- Q16.16.
- Hoặc format khác dựa trên dynamic range thực tế.

Không chuyển sang fixed-point trước khi có baseline float đáng tin cậy.

So sánh:
| Thuộc tính | Float | Fixed-point |
| :--- | :--- | :--- |
| Sai số | đo | đo |
| Execution time | đo | đo |
| Flash | đo | đo |
| RAM | đo | đo |
| Độ phức tạp | đánh giá | đánh giá |

---

## 10. Telemetry STM32 → PC

Không nên gửi text dài ở 500 Hz nếu gây blocking.

Packet nên chứa tối thiểu:
```text
timestamp
roll_acc
roll_gyro
roll_cf
roll_kalman
pitch_acc
pitch_gyro
pitch_cf
pitch_kalman
```

Có hai giai đoạn:
### Debug
Có thể dùng ASCII/CSV cho dễ nhìn.

### Final demo
Cân nhắc binary packet có:
```text
HEADER | LENGTH | TIMESTAMP | DATA | CHECKSUM
```
Python parse packet rồi cập nhật UI.

---

## 11. Python trên PC

Python **không thay STM32 chạy Kalman**.

Python đảm nhiệm:
### Realtime Plot
Hiển thị đồng thời:
- Accelerometer angle.
- Gyro angle.
- Complementary.
- Kalman.

### 3D Visualizer
Dùng:
```text
Roll_Kalman
Pitch_Kalman
```
để xoay mô hình 3D.

### Data Logger
Lưu CSV:
```text
timestamp,
roll_acc,
roll_gyro,
roll_cf,
roll_kalman,
pitch_acc,
...
```

### Offline Analysis
Tính:
- Mean error.
- MAE.
- RMSE.
- Standard deviation.
- Drift.
- Settling time nếu xây được quy trình đo phù hợp.
- Execution-time statistics.

---

## 12. Hệ thống đối chứng

Để đạt phần đánh giá định lượng, cần có góc tham chiếu.

Một phương án:
```text
       Servo / bàn quay
              │
       Reference angle
              │
              ▼
          MPU6500
              │
              ▼
           STM32
              │
              ▼
      Estimated angle
              │
              ▼
           Python
              │
              ▼
 Error = Estimate - Reference
```
Servo không mặc nhiên là chuẩn tuyệt đối; nếu dùng servo làm đối chứng phải đánh giá backlash, độ chính xác vị trí và cơ khí gá đặt.

---

## 13. Bộ thí nghiệm đề xuất

### Test 1 -- Static Accuracy
Các góc ví dụ:
```text
-60°
-45°
-30°
0°
+30°
+45°
+60°
```
Đo sai số từng phương pháp.

### Test 2 -- Long-term Drift
Giữ hệ thống cố định trong ít nhất 5 phút.
Quan sát:
- Gyro-only drift.
- Complementary.
- Kalman.

### Test 3 -- Step/Rotation Response
Thay đổi góc có kiểm soát:
```text
0° → 30° → 60° → 0°
```
Đánh giá response.

### Test 4 -- Vibration
Tạo rung có kiểm soát. Quan sát ảnh hưởng lên accelerometer và khả năng fusion.

### Test 5 -- Dynamic Movement
Xoay cảm biến liên tục với nhiều tốc độ.

### Test 6 -- Runtime
Benchmark từng thuật toán.

### Test 7 -- Float vs Fixed-point
So sánh độ chính xác và thời gian thực thi.

---

## 14. Chỉ số đánh giá

Không chỉ trình bày đồ thị đẹp.

### Error
$$e_i = \theta_{estimated,i} - \theta_{reference,i}$$

### MAE
$$MAE = \frac{1}{N}\sum |e_i|$$

### RMSE
$$RMSE = \sqrt{\frac{1}{N}\sum e_i^2}$$

Ngoài ra:
- Bias.
- Standard deviation.
- Peak error.
- Drift theo thời gian.
- Execution time.
- Sampling jitter.

---

## 15. Các phase triển khai

### PHASE 0 -- Chốt phần cứng và tài liệu
- [ ] Xác nhận chính xác STM32 board/MCU.
- [ ] Xác nhận module là MPU6500.
- [ ] Lưu datasheet/reference manual cần dùng.
- [ ] Chốt pin map.
- [ ] Chốt nguồn 3.3 V/logic level.
- [ ] Chốt SPI instance.
- [ ] Chốt UART/debug path.
- [ ] Tạo Git repository.
- [ ] Viết README kiến trúc ban đầu.

**Gate:** pin map và electrical interface được kiểm tra.

---

### PHASE 1 -- Bring-up STM32 register-level
- [ ] RCC.
- [ ] GPIO.
- [ ] CS GPIO.
- [ ] SPI.
- [ ] Test SPI transfer cơ bản.

**Gate:** SPI clock/MOSI/CS đúng; nếu có logic analyzer/oscilloscope thì lưu waveform làm bằng chứng.

---

### PHASE 2 -- MPU6500 WHO_AM_I
- [ ] `ReadReg()`.
- [ ] `WriteReg()`.
- [ ] WHO_AM_I.
- [ ] Reset MPU.
- [ ] Kiểm chứng đọc lặp ổn định.

**Gate:** nhận đúng ID và không có lỗi ngẫu nhiên trong test lặp.

---

### PHASE 3 -- MPU6500 6-axis data
- [ ] Cấu hình gyro.
- [ ] Cấu hình accelerometer.
- [ ] DLPF.
- [ ] Sample rate.
- [ ] Burst read.
- [ ] Convert raw → physical unit.

**Gate:** dữ liệu phản ứng đúng khi xoay từng trục.

---

### PHASE 4 -- INT + sampling 500 Hz
- [ ] MPU Data Ready Interrupt.
- [ ] GPIO input.
- [ ] AFIO/EXTI.
- [ ] NVIC.
- [ ] ISR.
- [ ] Đo sampling period.
- [ ] Đo jitter.

**Gate:** sampling thực tế ổn định quanh 500 Hz.

---

### PHASE 5 -- Calibration
- [ ] Gyro bias.
- [ ] Acc 6-face.
- [ ] Offset.
- [ ] Scale.
- [ ] So sánh before/after.

**Gate:** calibration cải thiện số liệu theo metric đã chọn.

---

### PHASE 6 -- Baseline angle estimation
- [ ] Roll/Pitch accelerometer.
- [ ] Gyro integration.
- [ ] Kiểm tra sign/axis convention.
- [ ] Kiểm tra `dt`.

**Gate:** góc phản ứng đúng chiều và đúng gần giá trị tham chiếu.

---

### PHASE 7 -- Complementary Filter
- [ ] Viết filter.
- [ ] Tune alpha.
- [ ] Static test.
- [ ] Dynamic test.
- [ ] Vibration test.

**Gate:** có dữ liệu chứng minh ưu/nhược so với Acc/Gyro riêng.

---

### PHASE 8 -- Kalman float
- [ ] State model.
- [ ] Predict.
- [ ] Covariance prediction.
- [ ] Measurement update.
- [ ] Kalman gain.
- [ ] Bias estimation.
- [ ] Tune Q/R.
- [ ] Roll.
- [ ] Pitch.

**Gate:** kiểm chứng bằng dữ liệu thực, không chỉ "code chạy".

---

### PHASE 9 -- PC telemetry + Python
- [ ] USART register-level.
- [ ] Packet protocol.
- [ ] Python receiver.
- [ ] Realtime graph.
- [ ] CSV logger.
- [ ] 3D visualization.

**Gate:** demo liên tục, không làm hỏng sampling.

---

### PHASE 10 -- Benchmark
- [ ] Timer/cycle measurement.
- [ ] Acc calculation.
- [ ] Gyro integration.
- [ ] Complementary.
- [ ] Kalman.
- [ ] Total processing budget.

**Gate:**
$$T_{processing} < T_s$$
với margin hợp lý.

---

### PHASE 11 -- Fixed-point
- [ ] Chọn Q-format.
- [ ] Xác định range.
- [ ] Chống overflow.
- [ ] Port Complementary/Kalman theo phạm vi đề tài.
- [ ] So sánh float/fixed.

**Gate:** có bảng accuracy/runtime/resource.

---

### PHASE 12 -- Evaluation
- [ ] Static accuracy.
- [ ] Drift ≥ 5 phút.
- [ ] Dynamic response.
- [ ] Vibration.
- [ ] RMSE/MAE.
- [ ] Execution time.
- [ ] Sampling jitter.
- [ ] Float/fixed comparison.

---

### PHASE 13 -- Final demo
Demo nên cho người xem thấy theo thứ tự:
```text
1. Hardware
2. SPI register-level
3. WHO_AM_I
4. Raw Acc/Gyro
5. Calibration
6. Acc angle
7. Gyro angle
8. Complementary
9. Kalman
10. 4 curves realtime
11. 3D model
12. Reference-angle experiment
13. Error metrics
14. Runtime comparison
```

---

## 16. Phân chia công việc nhóm 4 người

Mục tiêu của cách chia này là:
- 4 thành viên có thể bắt đầu làm song song.
- Ranh giới module rõ ràng.
- Hạn chế conflict code.
- Dễ chứng minh đóng góp cá nhân trên Git/GitHub.
- Các module có interface thống nhất để tích hợp về sau.

### Kiến trúc phân công

```text
SV1                    SV2                    SV3                    SV4
STM32 Low-level        MPU6500/Acquisition   Algorithm              Python/PC
     │                       │                    │                     │
     │ SPI API               │ SensorData_t       │ Attitude_t          │
     ├──────────────────────►├───────────────────►├────────────────────►│
     │                       │                    │                     │
RCC / GPIO / SPI        MPU Driver          Acc/Gyro              Serial parser
Register-level          Burst Read          Complementary          Realtime Plot
                        EXTI/INT 500 Hz      Kalman                CSV Logger
                        Calibration          Benchmark             3D Visualizer
```

### SV1 -- STM32F103 Low-level / Register Layer
Phụ trách:
- RCC register-level.
- GPIO register-level.
- SPI register-level.
- CS/NCS.
- Clock/pin configuration.
- Các API SPI cơ bản.
- Kiểm tra waveform SPI nếu có oscilloscope/logic analyzer.

API bàn giao dự kiến:
```c
void SPI1_Init(void);
uint8_t SPI1_Transfer(uint8_t data);
void SPI1_Transmit(const uint8_t *data, uint16_t len);
```

**Deliverable chính:** tầng phần cứng STM32 hoạt động ổn định để SV2 xây MPU6500 driver phía trên.

### SV2 -- MPU6500 Driver / Data Acquisition
Phụ trách:
- MPU6500 register map.
- `ReadReg()` / `WriteReg()`.
- WHO_AM_I.
- Reset/config MPU6500.
- Accelerometer/Gyroscope range.
- DLPF.
- Sample rate.
- Burst Read.
- MPU6500 Data Ready.
- EXTI/NVIC.
- Sampling mục tiêu 500 Hz.
- Gyro bias calibration.
- Accelerometer 6-face calibration.
- Chuẩn hóa dữ liệu thành `SensorData_t`.

Interface bàn giao dự kiến:
```c
typedef struct
{
    float ax;
    float ay;
    float az;

    float gx;
    float gy;
    float gz;

    float dt;
    uint32_t timestamp;
} SensorData_t;
```
SV3 chỉ cần nhận `SensorData_t`, không cần biết SPI bên dưới hoạt động thế nào.

### SV3 -- Thuật toán / Sensor Fusion
SV3 **không chờ SV1/SV2 hoàn thành**.

Trong giai đoạn đầu, dùng dữ liệu giả hoặc dataset:
```text
time,ax,ay,az,gx,gy,gz
0.000,...
0.002,...
0.004,...
...
```

Phụ trách:
- Roll/Pitch từ accelerometer.
- Gyroscope integration.
- Complementary Filter.
- Kalman Filter 2-state tự viết.
- Tuning `Q/R`.
- Kalman bias estimation.
- Float implementation.
- Fixed-point implementation sau khi float đã PASS.
- Benchmark execution time.

API mục tiêu:
```c
typedef struct
{
    float roll_acc;
    float pitch_acc;

    float roll_gyro;
    float pitch_gyro;

    float roll_cf;
    float pitch_cf;

    float roll_kalman;
    float pitch_kalman;
} Attitude_t;

void Attitude_Update(
    const SensorData_t *sensor,
    Attitude_t *attitude
);
```

### SV4 -- Python / PC / Visualization
SV4 cũng **không chờ STM32 hoàn thành**.

Ngay từ đầu tạo fake packet theo format đã thống nhất.

Phụ trách:
- Serial receiver.
- Packet parser.
- Realtime plotting.
- CSV logger.
- Mô hình 3D.
- Hiển thị Roll/Pitch.
- Công cụ offline analysis.
- Hỗ trợ tạo bảng/đồ thị kết quả.

Format logic tối thiểu:
```text
timestamp,
roll_acc,
roll_gyro,
roll_cf,
roll_kalman,
pitch_acc,
pitch_gyro,
pitch_cf,
pitch_kalman
```

### Phần đánh giá/thực nghiệm
Ngay từ đầu phải định nghĩa protocol đo:
- Static angle test.
- Drift ≥ 5 phút.
- Dynamic response.
- Vibration.
- MAE.
- RMSE.
- Standard deviation.
- Execution time.
- Sampling jitter.
- Float vs fixed-point.

---

## 17. Git/GitHub Workflow -- mỗi sinh viên một branch

Để phù hợp cách quản lý đóng góp cá nhân, repository được tổ chức theo nguyên tắc:

```text
GitHub Repository
│
├── main
│    └── Phiên bản đã tích hợp và kiểm chứng
│
├── sv1-stm32-lowlevel
│    └── RCC / GPIO / SPI register-level
│
├── sv2-mpu6500-driver
│    └── MPU6500 / EXTI / Sampling / Calibration
│
├── sv3-algorithm
│    └── Acc / Gyro / Complementary / Kalman
│
└── sv4-python
     └── Serial / Plot / Logger / 3D
```

### Quy tắc branch
Mỗi thành viên làm việc chủ yếu trên branch cá nhân:
- SV1 → `sv1-stm32-lowlevel`
- SV2 → `sv2-mpu6500-driver`
- SV3 → `sv3-algorithm`
- SV4 → `sv4-python`

Không code trực tiếp lên `main` trong quá trình phát triển thông thường.

### Quy trình tích hợp
```text
Individual Branch
       │
       │ commit + test
       ▼
   Pull Request
       │
       │ review
       ▼
      main
       │
       ▼
Integration Test
```

### Commit ownership
Mỗi sinh viên:
- Dùng tài khoản GitHub của chính mình.
- Commit phần code mình thực hiện.
- Không để một người commit hộ toàn bộ nhóm.
- Commit nhỏ, có nội dung rõ ràng (`feat(...)`, `fix(...)`, `test(...)`).
- Có lịch sử phát triển liên tục thay vì dồn một commit cuối.

### Interface-first
- **SV1 → SV2:** `uint8_t SPI1_Transfer(uint8_t data);`
- **SV2 → SV3:** `SensorData_t`
- **SV3 → SV4:** `Attitude_t`
- **STM32 → Python:** `Telemetry Packet`

### Merge checkpoints
- **MERGE 1:** SV1: SPI register-level PASS | SV2: WHO_AM_I PASS
- **MERGE 2:** SV2: Raw 6-axis + 500 Hz PASS
- **MERGE 3:** SV3: Acc/Gyro/Complementary PASS
- **MERGE 4:** SV3: Kalman float PASS
- **MERGE 5:** SV4: Python Plot + Logger + 3D PASS
- **MERGE 6:** Full-system integration PASS
- **MERGE 7:** Fixed-point + benchmark + final evaluation

---

## 18. Những lỗi cần tránh

- Dùng MPU6050 code copy nguyên cho MPU6500 mà không kiểm tra register map.
- Dùng HAL ở mọi chỗ rồi chỉ sửa vài dòng register để "đủ yêu cầu".
- `HAL_Delay()` làm scheduler.
- Chạy Kalman trên Python thay vì STM32.
- Gửi quá nhiều ASCII ở 500 Hz gây blocking.
- Làm Kalman trước khi raw data/calibration đúng.
- Không kiểm tra `dt`.
- Tune Q/R để đồ thị "đẹp" nhưng không có quy trình.
- Chỉ demo bằng mắt, không có reference angle.
- Claim "Kalman tốt hơn" mà không có metric.
- Làm fixed-point quá sớm.
- ISR quá dài.
- Không kiểm soát hệ tọa độ/sign convention.
- Không lưu raw data để tái phân tích.

---

## 19. Definition of Done

Dự án được coi là hoàn thiện khi:
- [ ] MPU6500 giao tiếp SPI ổn định.
- [ ] Driver SPI/MPU6500 tự viết.
- [ ] Có phần register-level rõ ràng và giải thích được.
- [ ] Sampling 500 Hz được đo thực tế.
- [ ] Calibration hoàn thành.
- [ ] Acc angle chạy.
- [ ] Gyro integration chạy.
- [ ] Complementary chạy.
- [ ] Kalman 2-state tự viết chạy.
- [ ] Roll/Pitch được kiểm chứng.
- [ ] Python realtime plot chạy.
- [ ] 3D model chạy.
- [ ] Có data logging.
- [ ] Có reference-angle test.
- [ ] Có MAE/RMSE.
- [ ] Có drift test.
- [ ] Có vibration/dynamic test.
- [ ] Có execution-time benchmark.
- [ ] Có float/fixed-point comparison nếu thuộc yêu cầu cuối.
- [ ] Git thể hiện đóng góp từng thành viên.
- [ ] Báo cáo có sơ đồ khối, flowchart, register configuration và kết quả đo.
- [ ] Video demo hoàn chỉnh.

---

## 20. Thứ tự ưu tiên thực tế

```text
P0 – BẮT BUỘC
SPI register
→ WHO_AM_I
→ Burst Read
→ INT 500 Hz
→ Calibration
→ Acc/Gyro angle
→ Complementary
→ Kalman float
→ Python Plot
→ Đánh giá sai số

P1 – RẤT NÊN CÓ
3D visualization
→ Drift test
→ Vibration test
→ Runtime benchmark
→ Reference-angle rig

P2 – NÂNG CAO
Fixed-point
→ Binary telemetry
→ Interrupt/DMA UART
→ CPU/resource optimization
```

---

## 21. Milestone quan trọng nhất

```text
M0  Hardware + pin map
 ↓
M1  SPI register hoạt động
 ↓
M2  WHO_AM_I PASS
 ↓
M3  Raw 6-axis PASS
 ↓
M4  Data Ready 500 Hz PASS
 ↓
M5  Calibration PASS
 ↓
M6  Acc/Gyro baseline PASS
 ↓
M7  Complementary PASS
 ↓
M8  Kalman float PASS
 ↓
M9  Python realtime + 3D PASS
 ↓
M10 Quantitative evaluation PASS
 ↓
M11 Fixed-point + benchmark
 ↓
M12 Final demo/report
```

---

## 22. Kết quả cuối mong muốn

Khi bảo vệ, nhóm không chỉ nói:
> "MPU6500 đo góc và Kalman làm tín hiệu mượt hơn."

Mà phải chứng minh được toàn bộ chuỗi:
```text
Physical motion
      ↓
MPU6500
      ↓
SPI register-level
      ↓
Stable 500 Hz sampling
      ↓
Calibration
      ↓
Acc + Gyro estimates
      ↓
Complementary + Kalman
      ↓
Measured execution time
      ↓
STM32 → PC telemetry
      ↓
Realtime graph + 3D
      ↓
Reference measurement
      ↓
MAE / RMSE / Drift / Response
      ↓
Kết luận dựa trên số liệu
```
