#include "stm32f10x.h"
#include "mpu6500_i2c.h"

int16_t  raw_acc_i2c[3], raw_gyro_i2c[3];
float    acc_g_i2c[3], gyro_dps_i2c[3];
MPU_Calib_t calib_i2c;

/* Xử lý ngắt EXTI0 cho chân PB0 (Data Ready từ MPU6500, active-high) */
void EXTI0_IRQHandler(void) {
    if (EXTI->PR & EXTI_PR_PR0) {
        EXTI->PR = EXTI_PR_PR0;    /* Xóa cờ ngắt bằng cách ghi 1 vào bit */
        g_mpu_data_ready = 1;
    }
}

int main(void) {
    /* 1. Khởi tạo ngoại vi I2C1 (PB6/PB7) và ngắt EXTI0 (PB0) */
    MPU6500_I2C_LowLevel_Init();

    /* 2. Khởi tạo cảm biến MPU6500 - thử lại đến khi thành công
     *    g_mpu_id đã được gán bên trong MPU6500_Init_I2C, không đọc lại */
    while (!MPU6500_Init_I2C()) {
        delay_ms(100);
    }

    /* 3. Nạp thông số hiệu chuẩn Accel 6 mặt (đo thực tế trên phần cứng của bạn)
     *    Thứ tự tham số: ax_max, ax_min, ay_max, ay_min, az_max, az_min */
    MPU6500_CalibAccel6Face_I2C(&calib_i2c, 8200, -8180, 8190, -8210, 8300, -8100);

    /* 4. Lấy Bias Gyro (để mạch nằm yên hoàn toàn, lấy 500 mẫu @ 500Hz = 1 giây) */
    MPU6500_CalibGyro_I2C(&calib_i2c, 500);

    /* 5. Vòng lặp chính - đồng bộ theo cờ Data Ready từ ngắt EXTI0
     *    Cứ mỗi 2ms (500Hz) cảm biến kéo INT → ISR đặt cờ → đọc ngay
     *    dt = 0.002f khi dùng cho bộ lọc Madgwick / Complementary */
    while (1) {
        if (g_mpu_data_ready) {
            g_mpu_data_ready = 0;

            /* Đọc 14 byte dữ liệu thô, bỏ qua mẫu nếu I2C lỗi */
            if (MPU6500_ReadRaw_I2C(raw_acc_i2c, raw_gyro_i2c) == 0) {
                /* Chuyển đổi sang đơn vị vật lý (g và dps) */
                MPU6500_GetScaled_I2C(raw_acc_i2c, raw_gyro_i2c,
                                      &calib_i2c,
                                      acc_g_i2c, gyro_dps_i2c);

                /* ---- Thêm xử lý của bạn ở đây ----
                 * Ví dụ: bộ lọc Complementary / Madgwick với dt = 0.002f
                 * float pitch = ..., roll = ...;
                 * ----------------------------------- */
            }
        }
        /* Không có delay_ms: CPU nhàn rỗi cho đến khi có ngắt tiếp theo */
    }
}
