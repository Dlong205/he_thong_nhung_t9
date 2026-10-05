#include "mpu6500_i2c.h"

volatile uint8_t g_mpu_data_ready = 0;
volatile uint8_t g_mpu_id = 0;

#define I2C_TIMEOUT_MAX 20000

/* Hàm delay thô sơ (xấp xỉ, dùng cho init không cần chính xác) */
void delay_ms(volatile uint32_t ms) {
    ms *= 8000;
    while(ms--) {
        __NOP();
    }
}

/* ====================================================================
 * Phục hồi bus I2C khi slave đang kéo SDA thấp (MCU reset giữa chừng)
 * Quy trình: Tắt I2C → GPIO open-drain → 9 xung SCL → STOP → SWRST
 * ==================================================================== */
static void I2C_Bus_Reset(void) {
    /* 1. Tắt I2C peripheral */
    I2C1->CR1 &= ~I2C_CR1_PE;

    /* 2. Chuyển PB6 (SCL) và PB7 (SDA) sang GPIO Output Open-Drain */
    /* CNF=01 (OD), MODE=11 (50MHz) → giá trị nibble = 0x7 */
    GPIOB->CRL &= ~(0xFF000000);
    GPIOB->CRL |=  (0x77000000);  /* GPIO_OD 50MHz cho PB6, PB7 */

    /* Set cả hai lên HIGH qua ODR */
    GPIOB->BSRR = (1 << 6) | (1 << 7);

    /* 3. Xung 9 lần trên SCL để giải phóng slave đang kéo SDA */
    for (int i = 0; i < 9; i++) {
        GPIOB->BRR  = (1 << 6);   /* SCL LOW  */
        delay_ms(1);
        GPIOB->BSRR = (1 << 6);   /* SCL HIGH */
        delay_ms(1);
    }

    /* 4. Tạo điều kiện STOP: SDA LOW → SCL HIGH → SDA HIGH */
    GPIOB->BRR  = (1 << 7);   /* SDA LOW  */
    delay_ms(1);
    GPIOB->BSRR = (1 << 6);   /* SCL HIGH */
    delay_ms(1);
    GPIOB->BSRR = (1 << 7);   /* SDA HIGH */
    delay_ms(1);

    /* 5. Chuyển PB6, PB7 về Alternate Function Open-Drain 50MHz */
    /* CNF=11 (AF_OD), MODE=11 (50MHz) → giá trị nibble = 0xF */
    GPIOB->CRL &= ~(0xFF000000);
    GPIOB->CRL |=  (0xFF000000);

    /* 6. SWRST để reset toàn bộ trạng thái I2C peripheral */
    I2C1->CR1 |= I2C_CR1_SWRST;
    delay_ms(2);
    I2C1->CR1 &= ~I2C_CR1_SWRST;

    /* 7. Cấu hình lại I2C: Fast Mode 400kHz, APB1 = 36MHz */
    I2C1->CR2   = 36;                 /* FREQ = 36MHz           */
    I2C1->CCR   = I2C_CCR_FS | 30;   /* Fast Mode: T_high=30   */
    I2C1->TRISE = 11;                 /* TRISE = (300ns/27.7ns)+1 */
    I2C1->CR1  |= I2C_CR1_PE;        /* Bật I2C1               */
}

/* Khởi tạo cấu hình I2C1 (PB6=SCL, PB7=SDA) và ngắt EXTI0 (PB0) */
void MPU6500_I2C_LowLevel_Init(void) {
    /* 1. Bật clock cho GPIOB, AFIO và I2C1 */
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN | RCC_APB2ENR_AFIOEN;
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;

    /* 2. Cấu hình PB6, PB7 làm Alternate Function Open-Drain 50MHz */
    GPIOB->CRL &= ~(0xFF000000);   /* Xóa cấu hình PB6, PB7 */
    GPIOB->CRL |=  (0xFF000000);   /* AF_OD (F), Output 50MHz */

    /* 3. Cấu hình PB0 làm Input Pull-DOWN cho chân INT (active-high, push-pull) */
    GPIOB->CRL &= ~(0x0000000F);   /* Xóa cấu hình PB0 */
    GPIOB->CRL |=  (0x00000008);   /* Input Pull-up/down */
    GPIOB->BRR   =  (1 << 0);      /* Pull-DOWN: ODR bit0 = 0 */

    /* 4. Reset & Cấu hình I2C1 */
    I2C_Bus_Reset();

    /* 5. Cấu hình ngắt EXTI0 trên PB0 */
    AFIO->EXTICR[0] &= ~AFIO_EXTICR1_EXTI0;
    AFIO->EXTICR[0] |= AFIO_EXTICR1_EXTI0_PB; /* Map EXTI0 to PB0 */

    EXTI->IMR  |= EXTI_IMR_MR0;    /* Unmask EXTI0 */
    EXTI->RTSR |= EXTI_RTSR_TR0;   /* Kích hoạt ngắt sườn lên (INT active-high) */

    NVIC_EnableIRQ(EXTI0_IRQn);
    NVIC_SetPriority(EXTI0_IRQn, 1);
}

/* ====================================================================
 * I2C_WriteReg: Ghi thanh ghi, trả về 0=OK, 1=lỗi timeout
 * ==================================================================== */
uint8_t I2C_WriteReg(uint8_t dev_addr, uint8_t reg_addr, uint8_t data) {
    uint32_t timeout;

    timeout = I2C_TIMEOUT_MAX;
    while ((I2C1->SR2 & I2C_SR2_BUSY) && --timeout);
    if (!timeout) { I2C_Bus_Reset(); return 1; }

    I2C1->CR1 |= I2C_CR1_START;
    timeout = I2C_TIMEOUT_MAX;
    while (!(I2C1->SR1 & I2C_SR1_SB) && --timeout);
    if (!timeout) { I2C_Bus_Reset(); return 1; }

    I2C1->DR = dev_addr;
    timeout = I2C_TIMEOUT_MAX;
    while (!(I2C1->SR1 & I2C_SR1_ADDR) && --timeout);
    if (!timeout) { I2C_Bus_Reset(); return 1; }
    (void)I2C1->SR2; /* Clear ADDR */

    I2C1->DR = reg_addr;
    timeout = I2C_TIMEOUT_MAX;
    while (!(I2C1->SR1 & I2C_SR1_TXE) && --timeout);
    if (!timeout) { I2C_Bus_Reset(); return 1; }

    I2C1->DR = data;
    /* Chờ BTF để đảm bảo cả hai byte đã được truyền xong trước khi STOP */
    timeout = I2C_TIMEOUT_MAX;
    while (!(I2C1->SR1 & I2C_SR1_BTF) && --timeout);
    if (!timeout) { I2C_Bus_Reset(); return 1; }

    I2C1->CR1 |= I2C_CR1_STOP;
    return 0;
}

/* ====================================================================
 * I2C_ReadReg: Đọc 1 thanh ghi, trả về giá trị (0 nếu lỗi)
 * ==================================================================== */
uint8_t I2C_ReadReg(uint8_t dev_addr, uint8_t reg_addr) {
    uint8_t  data = 0;
    uint32_t timeout;

    timeout = I2C_TIMEOUT_MAX;
    while ((I2C1->SR2 & I2C_SR2_BUSY) && --timeout);
    if (!timeout) { I2C_Bus_Reset(); return 0; }

    /* --- Phase Write: gửi địa chỉ thanh ghi --- */
    I2C1->CR1 |= I2C_CR1_START;
    timeout = I2C_TIMEOUT_MAX;
    while (!(I2C1->SR1 & I2C_SR1_SB) && --timeout);
    if (!timeout) { I2C_Bus_Reset(); return 0; }

    I2C1->DR = dev_addr;
    timeout = I2C_TIMEOUT_MAX;
    while (!(I2C1->SR1 & I2C_SR1_ADDR) && --timeout);
    if (!timeout) { I2C_Bus_Reset(); return 0; }
    (void)I2C1->SR2;

    I2C1->DR = reg_addr;
    /* Chờ BTF trước Restart để tránh điều kiện race */
    timeout = I2C_TIMEOUT_MAX;
    while (!(I2C1->SR1 & I2C_SR1_BTF) && --timeout);
    if (!timeout) { I2C_Bus_Reset(); return 0; }

    /* --- Phase Read: Repeated START --- */
    I2C1->CR1 |= I2C_CR1_START;
    timeout = I2C_TIMEOUT_MAX;
    while (!(I2C1->SR1 & I2C_SR1_SB) && --timeout);
    if (!timeout) { I2C_Bus_Reset(); return 0; }

    I2C1->DR = dev_addr | 0x01;
    timeout = I2C_TIMEOUT_MAX;
    while (!(I2C1->SR1 & I2C_SR1_ADDR) && --timeout);
    if (!timeout) { I2C_Bus_Reset(); return 0; }

    /* Theo RM0008: NACK trước khi xóa ADDR, rồi mới set STOP */
    I2C1->CR1 &= ~I2C_CR1_ACK;
    (void)I2C1->SR2;             /* Clear ADDR */
    I2C1->CR1 |= I2C_CR1_STOP;

    timeout = I2C_TIMEOUT_MAX;
    while (!(I2C1->SR1 & I2C_SR1_RXNE) && --timeout);
    if (!timeout) { I2C_Bus_Reset(); return 0; }
    data = I2C1->DR;

    return data;
}

/* ====================================================================
 * I2C_ReadBurst: Đọc nhiều byte, trả về 0=OK, 1=lỗi
 * Xử lý byte N-2/N-1 theo RM0008 (chờ BTF, tránh race condition)
 * ==================================================================== */
uint8_t I2C_ReadBurst(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint16_t len) {
    uint32_t timeout;

    if (len == 0) return 1;

    timeout = I2C_TIMEOUT_MAX;
    while ((I2C1->SR2 & I2C_SR2_BUSY) && --timeout);
    if (!timeout) { I2C_Bus_Reset(); return 1; }

    /* --- Phase Write: gửi địa chỉ thanh ghi --- */
    I2C1->CR1 |= I2C_CR1_START;
    timeout = I2C_TIMEOUT_MAX;
    while (!(I2C1->SR1 & I2C_SR1_SB) && --timeout);
    if (!timeout) { I2C_Bus_Reset(); return 1; }

    I2C1->DR = dev_addr;
    timeout = I2C_TIMEOUT_MAX;
    while (!(I2C1->SR1 & I2C_SR1_ADDR) && --timeout);
    if (!timeout) { I2C_Bus_Reset(); return 1; }
    (void)I2C1->SR2;

    I2C1->DR = reg_addr;
    /* Chờ BTF trước Restart */
    timeout = I2C_TIMEOUT_MAX;
    while (!(I2C1->SR1 & I2C_SR1_BTF) && --timeout);
    if (!timeout) { I2C_Bus_Reset(); return 1; }

    /* --- Phase Read: Repeated START --- */
    I2C1->CR1 |= I2C_CR1_START;
    timeout = I2C_TIMEOUT_MAX;
    while (!(I2C1->SR1 & I2C_SR1_SB) && --timeout);
    if (!timeout) { I2C_Bus_Reset(); return 1; }

    I2C1->DR = dev_addr | 0x01;
    timeout = I2C_TIMEOUT_MAX;
    while (!(I2C1->SR1 & I2C_SR1_ADDR) && --timeout);
    if (!timeout) { I2C_Bus_Reset(); return 1; }

    if (len == 1) {
        /* 1 byte: NACK → Clear ADDR → STOP → đọc */
        I2C1->CR1 &= ~I2C_CR1_ACK;
        (void)I2C1->SR2;
        I2C1->CR1 |= I2C_CR1_STOP;
        timeout = I2C_TIMEOUT_MAX;
        while (!(I2C1->SR1 & I2C_SR1_RXNE) && --timeout);
        if (!timeout) { I2C_Bus_Reset(); return 1; }
        data[0] = I2C1->DR;

    } else if (len == 2) {
        /* 2 byte: POS+NACK trước Clear ADDR, chờ BTF, đọc cả hai */
        I2C1->CR1 |= I2C_CR1_POS;
        I2C1->CR1 &= ~I2C_CR1_ACK;
        (void)I2C1->SR2;
        timeout = I2C_TIMEOUT_MAX;
        while (!(I2C1->SR1 & I2C_SR1_BTF) && --timeout);
        if (!timeout) { I2C1->CR1 &= ~I2C_CR1_POS; I2C_Bus_Reset(); return 1; }
        I2C1->CR1 |= I2C_CR1_STOP;
        data[0] = I2C1->DR;
        data[1] = I2C1->DR;
        I2C1->CR1 &= ~I2C_CR1_POS;

    } else {
        /* N >= 3 byte: ACK → Clear ADDR → nhận từng byte */
        I2C1->CR1 |= I2C_CR1_ACK;
        (void)I2C1->SR2;

        for (uint16_t i = 0; i < len; i++) {
            if (i == len - 3) {
                /* Byte N-3 đã vào DR, chờ BTF → cả N-2 và N-3 đã có trong shift reg */
                timeout = I2C_TIMEOUT_MAX;
                while (!(I2C1->SR1 & I2C_SR1_BTF) && --timeout);
                if (!timeout) { I2C_Bus_Reset(); return 1; }
                /* Tắt ACK (sẽ NACK cho byte N-1) */
                I2C1->CR1 &= ~I2C_CR1_ACK;
                data[i] = I2C1->DR; /* Đọc byte N-3 */

            } else if (i == len - 2) {
                /* Chờ BTF: byte N-2 trong DR, byte N-1 trong shift register */
                timeout = I2C_TIMEOUT_MAX;
                while (!(I2C1->SR1 & I2C_SR1_BTF) && --timeout);
                if (!timeout) { I2C_Bus_Reset(); return 1; }
                I2C1->CR1 |= I2C_CR1_STOP;  /* STOP trước khi đọc byte N-2 */
                data[i] = I2C1->DR;          /* Đọc byte N-2 */

            } else if (i == len - 1) {
                /* Đọc byte cuối N-1 */
                timeout = I2C_TIMEOUT_MAX;
                while (!(I2C1->SR1 & I2C_SR1_RXNE) && --timeout);
                if (!timeout) { I2C_Bus_Reset(); return 1; }
                data[i] = I2C1->DR;

            } else {
                /* Các byte thường: chờ RXNE rồi đọc */
                timeout = I2C_TIMEOUT_MAX;
                while (!(I2C1->SR1 & I2C_SR1_RXNE) && --timeout);
                if (!timeout) { I2C_Bus_Reset(); return 1; }
                data[i] = I2C1->DR;
            }
        }
    }
    return 0;
}

/* ====================================================================
 * MPU6500_Init_I2C: Khởi tạo cảm biến, trả về 1=OK, 0=lỗi
 * ==================================================================== */
uint8_t MPU6500_Init_I2C(void) {
    delay_ms(100);
    g_mpu_id = I2C_ReadReg(MPU6500_I2C_ADDRESS, 0x75);

    /* Hỗ trợ MPU6500 (0x70), MPU9250 (0x71), MPU6500 variant (0x73) hoặc MPU6050 (0x68) */
    if (g_mpu_id != 0x70 && g_mpu_id != 0x71 && g_mpu_id != 0x73 && g_mpu_id != 0x68) {
        return 0;
    }

    /* Reset toàn bộ thanh ghi */
    if (I2C_WriteReg(MPU6500_I2C_ADDRESS, 0x6B, 0x80)) return 0;
    delay_ms(100);

    /* Chọn nguồn clock tự động (PLL với gyro) */
    if (I2C_WriteReg(MPU6500_I2C_ADDRESS, 0x6B, 0x01)) return 0;
    delay_ms(10);

    /* Sample Rate = 500Hz: SMPLRT_DIV = 1 (với DLPF bật: ODR = 1000/(1+1) = 500Hz) */
    if (I2C_WriteReg(MPU6500_I2C_ADDRESS, 0x19, 0x01)) return 0;
    /* DLPF = 41Hz (config 0x03), giảm nhiễu tần số cao */
    if (I2C_WriteReg(MPU6500_I2C_ADDRESS, 0x1A, 0x03)) return 0;
    /* Gyro: ±500 dps */
    if (I2C_WriteReg(MPU6500_I2C_ADDRESS, 0x1B, 0x08)) return 0;
    /* Accel: ±4g */
    if (I2C_WriteReg(MPU6500_I2C_ADDRESS, 0x1C, 0x08)) return 0;
    /* Bật ngắt Data Ready trên chân INT */
    if (I2C_WriteReg(MPU6500_I2C_ADDRESS, 0x38, 0x01)) return 0;
    
    /* QUAN TRỌNG: Cấu hình INT_PIN_CFG (0x37)
     * Ghi 0x10 (INT_ANYRD_2CLEAR) để cờ ngắt tự động xóa khi ta đọc bất kỳ thanh ghi nào (vd: đọc Data)
     * Nếu không có dòng này, chân INT có thể bị treo hoặc không phát xung mới! */
    if (I2C_WriteReg(MPU6500_I2C_ADDRESS, 0x37, 0x10)) return 0;

    return 1;
}

/* ====================================================================
 * MPU6500_ReadRaw_I2C: Đọc 14 byte, trả về 0=OK, 1=lỗi I2C
 * ==================================================================== */
uint8_t MPU6500_ReadRaw_I2C(int16_t acc[3], int16_t gyro[3]) {
    uint8_t buffer[14];

    if (I2C_ReadBurst(MPU6500_I2C_ADDRESS, 0x3B, buffer, 14) != 0) {
        return 1; /* Lỗi I2C: bỏ mẫu, giữ nguyên giá trị cũ */
    }

    acc[0]  = (int16_t)((buffer[0]  << 8) | buffer[1]);
    acc[1]  = (int16_t)((buffer[2]  << 8) | buffer[3]);
    acc[2]  = (int16_t)((buffer[4]  << 8) | buffer[5]);
    /* buffer[6..7] = TEMP_OUT, bỏ qua */
    gyro[0] = (int16_t)((buffer[8]  << 8) | buffer[9]);
    gyro[1] = (int16_t)((buffer[10] << 8) | buffer[11]);
    gyro[2] = (int16_t)((buffer[12] << 8) | buffer[13]);

    return 0;
}

void MPU6500_GetScaled_I2C(int16_t raw_acc[3], int16_t raw_gyro[3],
                           const MPU_Calib_t *calib,
                           float acc_g[3], float gyro_dps[3]) {
    for (int i = 0; i < 3; i++) {
        float a = ((float)raw_acc[i] - calib->acc_offset[i]) * calib->acc_scale[i];
        acc_g[i] = a / ACCEL_SCALE_4G;

        gyro_dps[i] = ((float)raw_gyro[i] - calib->gyro_bias[i]) / GYRO_SCALE_500DPS;
    }
}

void MPU6500_CalibAccel6Face_I2C(MPU_Calib_t *calib,
                                 float ax_max, float ax_min,
                                 float ay_max, float ay_min,
                                 float az_max, float az_min) {
    calib->acc_offset[0] = (ax_max + ax_min) / 2.0f;
    calib->acc_offset[1] = (ay_max + ay_min) / 2.0f;
    calib->acc_offset[2] = (az_max + az_min) / 2.0f;

    calib->acc_scale[0] = ACCEL_SCALE_4G / ((ax_max - ax_min) / 2.0f);
    calib->acc_scale[1] = ACCEL_SCALE_4G / ((ay_max - ay_min) / 2.0f);
    calib->acc_scale[2] = ACCEL_SCALE_4G / ((az_max - az_min) / 2.0f);
}

/* ====================================================================
 * MPU6500_CalibGyro_I2C: Lấy trung bình Bias Gyro (mạch để yên)
 * Dùng cờ data-ready, không dùng delay_ms thừa
 * ==================================================================== */
void MPU6500_CalibGyro_I2C(MPU_Calib_t *calib, uint16_t samples) {
    int32_t  sum[3] = {0, 0, 0};
    int16_t  raw_a[3], raw_g[3];

    for (uint16_t i = 0; i < samples; i++) {
        /* Chờ cờ Data Ready từ EXTI (có timeout phòng chân INT không nối) */
        uint32_t timeout = 100000;
        while (!g_mpu_data_ready && --timeout);
        g_mpu_data_ready = 0;

        if (MPU6500_ReadRaw_I2C(raw_a, raw_g) == 0) {
            /* Chỉ cộng mẫu khi đọc I2C thành công */
            sum[0] += raw_g[0];
            sum[1] += raw_g[1];
            sum[2] += raw_g[2];
        }
        /* Không có delay_ms ở đây: đã đồng bộ bằng cờ data-ready */
    }

    calib->gyro_bias[0] = (float)sum[0] / samples;
    calib->gyro_bias[1] = (float)sum[1] / samples;
    calib->gyro_bias[2] = (float)sum[2] / samples;
}
