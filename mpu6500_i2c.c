#include "mpu6500_i2c.h"
#include <math.h>
#include <stddef.h>

volatile uint8_t g_mpu_data_ready = 0;
volatile uint8_t g_mpu_id = 0;

#define I2C_TIMEOUT_MAX 20000

/* TIM2 is reserved by this standalone example: 1 MHz, 16-bit + overflow. */
static volatile uint32_t timer_high;
volatile uint32_t g_mpu_irq_count;
volatile uint32_t g_mpu_irq_time_us;
volatile uint32_t g_mpu_dropped;
static uint32_t consumed_irq, last_sample_us;
static uint8_t have_previous;

void TIM2_IRQHandler(void) {
    if (TIM2->SR & TIM_SR_UIF) {
        TIM2->SR = (uint16_t)~TIM_SR_UIF;
        timer_high += 65536u;
    }
}

uint32_t MPU6500_TimeUs(void) {
    uint32_t mask = __get_PRIMASK();
    uint32_t high, low;
    __disable_irq();
    high = timer_high;
    low = TIM2->CNT;
    if (TIM2->SR & TIM_SR_UIF) {
        high += 65536u;
        low = TIM2->CNT;
    }
    __set_PRIMASK(mask);
    return high + low;
}

void delay_ms(volatile uint32_t ms) {
    while (ms--) {
        uint32_t start = MPU6500_TimeUs();
        while ((uint32_t)(MPU6500_TimeUs() - start) < 1000u) {}
    }
}

/* Call only from the EXTI0 handler after clearing EXTI->PR. */
void MPU6500_OnDataReady(void) {
    g_mpu_irq_time_us = MPU6500_TimeUs();
    g_mpu_irq_count++;
    g_mpu_data_ready = 1;
}

void MPU6500_ResetAcquisition(void) {
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    consumed_irq = g_mpu_irq_count;
    g_mpu_data_ready = 0;
    __set_PRIMASK(mask);
    have_previous = 0;
}

/* Claim the latest event atomically; no FIFO, old samples cannot be recovered. */
static uint8_t take_event(uint32_t *stamp, uint32_t *sequence) {
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    *sequence = g_mpu_irq_count;
    if (*sequence == consumed_irq) {
        __set_PRIMASK(mask);
        return 0;
    }
    g_mpu_dropped += (uint32_t)(*sequence - consumed_irq) - 1u;
    consumed_irq = *sequence;
    *stamp = g_mpu_irq_time_us;
    g_mpu_data_ready = 0;
    __set_PRIMASK(mask);
    return 1;
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
    GPIOB->BRR  = (1 << 6);   /* SCL LOW before changing SDA */
    delay_ms(1);
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
uint8_t MPU6500_I2C_LowLevel_Init(void) {
    SystemCoreClockUpdate();
    /* The supplied project must configure HCLK=72 MHz and APB1=/2. */
    if (SystemCoreClock != 72000000u || ((RCC->CFGR >> 8) & 7u) != 4u)
        return 0;
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    TIM2->CR1 = 0;
    TIM2->PSC = 71;
    TIM2->ARR = 65535;
    TIM2->EGR = TIM_EGR_UG;
    TIM2->SR = 0;
    timer_high = 0;
    TIM2->CNT = 0;
    TIM2->DIER = TIM_DIER_UIE;
    NVIC_SetPriority(TIM2_IRQn, 0);
    NVIC_EnableIRQ(TIM2_IRQn);
    TIM2->CR1 = TIM_CR1_CEN;
    /* 1. Bật clock cho GPIOB, AFIO và I2C1 */
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN | RCC_APB2ENR_AFIOEN;
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;

    AFIO->MAPR &= ~AFIO_MAPR_I2C1_REMAP;

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

    EXTI->FTSR &= ~EXTI_FTSR_TR0;
    EXTI->PR = EXTI_PR_PR0;
    NVIC_SetPriority(EXTI0_IRQn, 1);
    NVIC_EnableIRQ(EXTI0_IRQn);
    return 1;
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
 * I2C_ReadReg: 0=OK, 1=error; value is written on success
 * ==================================================================== */
uint8_t I2C_ReadReg(uint8_t dev_addr, uint8_t reg_addr, uint8_t *value) {
    return I2C_ReadBurst(dev_addr, reg_addr, value, 1);
}

/* ====================================================================
 * I2C_ReadBurst: Đọc nhiều byte, trả về 0=OK, 1=lỗi
 * Xử lý byte N-2/N-1 theo RM0008 (chờ BTF, tránh race condition)
 * ==================================================================== */
uint8_t I2C_ReadBurst(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint16_t len) {
    uint32_t timeout;

    if (len == 0 || data == NULL) return 1;
    I2C1->CR1 &= ~I2C_CR1_POS;
    I2C1->CR1 |= I2C_CR1_ACK;
    if (len == 2) I2C1->CR1 |= I2C_CR1_POS;

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
        uint32_t mask = __get_PRIMASK();
        __disable_irq();
        I2C1->CR1 &= ~I2C_CR1_ACK;
        (void)I2C1->SR2;
        I2C1->CR1 |= I2C_CR1_STOP;
        __set_PRIMASK(mask);
        timeout = I2C_TIMEOUT_MAX;
        while (!(I2C1->SR1 & I2C_SR1_RXNE) && --timeout);
        if (!timeout) { I2C_Bus_Reset(); return 1; }
        data[0] = I2C1->DR;

    } else if (len == 2) {
        /* POS was set before address; clear ADDR then ACK atomically. */
        uint32_t mask = __get_PRIMASK();
        __disable_irq();
        (void)I2C1->SR2;
        I2C1->CR1 &= ~I2C_CR1_ACK;
        __set_PRIMASK(mask);
        timeout = I2C_TIMEOUT_MAX;
        while (!(I2C1->SR1 & I2C_SR1_BTF) && --timeout);
        if (!timeout) { I2C1->CR1 &= ~I2C_CR1_POS; I2C_Bus_Reset(); return 1; }
        mask = __get_PRIMASK();
        __disable_irq();
        I2C1->CR1 |= I2C_CR1_STOP;
        data[0] = I2C1->DR;
        data[1] = I2C1->DR;
        __set_PRIMASK(mask);
        I2C1->CR1 &= ~I2C_CR1_POS;

    } else {
        /* N >= 3 byte: ACK → Clear ADDR → nhận từng byte */
        I2C1->CR1 |= I2C_CR1_ACK;
        (void)I2C1->SR2;

        uint16_t i = 0;
        while (len - i > 3) {
            timeout = I2C_TIMEOUT_MAX;
            while (!(I2C1->SR1 & I2C_SR1_RXNE) && --timeout) {}
            if (!timeout) { I2C_Bus_Reset(); return 1; }
            data[i++] = I2C1->DR;
        }
        timeout = I2C_TIMEOUT_MAX;
        while (!(I2C1->SR1 & I2C_SR1_BTF) && --timeout) {}
        if (!timeout) { I2C_Bus_Reset(); return 1; }
        {
            uint32_t mask = __get_PRIMASK();
            __disable_irq();
            I2C1->CR1 &= ~I2C_CR1_ACK;
            data[i++] = I2C1->DR;
            __set_PRIMASK(mask);
        }
        timeout = I2C_TIMEOUT_MAX;
        while (!(I2C1->SR1 & I2C_SR1_BTF) && --timeout) {}
        if (!timeout) { I2C_Bus_Reset(); return 1; }
        {
            uint32_t mask = __get_PRIMASK();
            __disable_irq();
            I2C1->CR1 |= I2C_CR1_STOP;
            data[i++] = I2C1->DR;
            data[i] = I2C1->DR;
            __set_PRIMASK(mask);
        }
    }
    I2C1->CR1 &= ~I2C_CR1_POS;
    I2C1->CR1 |= I2C_CR1_ACK;
    return 0;
}

/* ====================================================================
 * MPU6500_Init_I2C: Khởi tạo cảm biến, trả về 1=OK, 0=lỗi
 * ==================================================================== */
uint8_t MPU6500_Init_I2C(void) {
    /* MPU6500 only. Other chips need their own verified register configuration. */
    static const uint8_t setup[][2] = {
        {0x6B, 0x01}, /* PLL clock, awake */
        {0x6C, 0x00}, /* enable all six axes */
        {0x19, 0x01}, /* 1 kHz / (1+1) = 500 Hz */
        {0x1A, 0x03}, /* gyro DLPF 41 Hz */
        {0x1B, 0x08}, /* gyro +/-500 dps, FCHOICE_B=0 */
        {0x1C, 0x08}, /* accel +/-4 g */
        {0x1D, 0x03}, /* accel DLPF 41 Hz */
        {0x37, 0x00}, /* INT active high, push-pull, pulse */
        {0x38, 0x01}  /* raw data ready interrupt */
    };
    uint8_t value;
    delay_ms(100);
    if (I2C_ReadReg(MPU6500_I2C_ADDRESS, 0x75, &value)) return 0;
    g_mpu_id = value;
    if (g_mpu_id != 0x70) return 0;
    if (I2C_WriteReg(MPU6500_I2C_ADDRESS, 0x6B, 0x80)) return 0;
    delay_ms(100);
    for (unsigned i = 0; i < sizeof(setup)/sizeof(setup[0]); ++i) {
        if (I2C_WriteReg(MPU6500_I2C_ADDRESS, setup[i][0], setup[i][1])) return 0;
        delay_ms(1);
        if (I2C_ReadReg(MPU6500_I2C_ADDRESS, setup[i][0], &value)) return 0;
        if (value != setup[i][1]) return 0;
    }
    delay_ms(100); /* sensor settling before calibration */
    MPU6500_ResetAcquisition();
    return 1;
}

/* ====================================================================
 * MPU6500_ReadRaw_I2C: Đọc 14 byte, trả về 0=OK, 1=lỗi I2C
 * ==================================================================== */
uint8_t MPU6500_ReadRaw_I2C(int16_t acc[3], int16_t gyro[3]) {
    uint8_t buffer[14];
    if (acc == NULL || gyro == NULL) return 1;

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

void MPU6500_CalibDefault(MPU_Calib_t *calib) {
    for (unsigned i = 0; i < 3; ++i) {
        calib->acc_offset[i] = 0.0f;
        calib->acc_scale[i] = 1.0f;
        calib->gyro_bias[i] = 0.0f;
    }
}

uint8_t MPU6500_CalibAccel6Face_I2C(MPU_Calib_t *calib,
    float ax_max, float ax_min, float ay_max, float ay_min,
    float az_max, float az_min) {
    float hi[3] = {ax_max, ay_max, az_max};
    float lo[3] = {ax_min, ay_min, az_min};
    if (calib == NULL) return 0;
    /* Inputs are stationary means from +/-1 g poses, NOT random extrema. */
    for (unsigned i = 0; i < 3; ++i) {
        if (!isfinite(hi[i]) || !isfinite(lo[i]) ||
            hi[i] < 0.5f * ACCEL_SCALE_4G ||
            lo[i] > -0.5f * ACCEL_SCALE_4G ||
            hi[i] > 1.5f * ACCEL_SCALE_4G ||
            lo[i] < -1.5f * ACCEL_SCALE_4G) return 0;
    }
    for (unsigned i = 0; i < 3; ++i) {
        calib->acc_offset[i] = (hi[i] + lo[i]) * 0.5f;
        calib->acc_scale[i] = 2.0f * ACCEL_SCALE_4G / (hi[i] - lo[i]);
    }
    return 1;
}

/* 1=success, 0=failure; never change bias on a failed calibration.
 * Blocking startup operation. Keep board stationary; IRQs must be enabled.
 */
uint8_t MPU6500_CalibGyro_I2C(MPU_Calib_t *calib, uint16_t samples) {
    float mean[3] = {0}, m2[3] = {0};
    int16_t a[3], g[3];
    uint32_t stamp, sequence;
    if (calib == NULL || samples < 2) return 0;
    MPU6500_ResetAcquisition();
    for (uint32_t n = 1; n <= samples; ++n) {
        uint32_t start = MPU6500_TimeUs();
        while (!take_event(&stamp, &sequence)) {
            if ((uint32_t)(MPU6500_TimeUs()-start) > 50000u) return 0;
        }
        if (MPU6500_ReadRaw_I2C(a, g)) return 0;
        if (g_mpu_irq_count != sequence) return 0;
        /* A simple motion screen, not proof of stationarity. */
        float norm2 = 0.0f;
        for (unsigned i = 0; i < 3; ++i) {
            float ag = a[i] / ACCEL_SCALE_4G;
            float delta = (float)g[i] - mean[i];
            norm2 += ag * ag;
            mean[i] += delta / (float)n;
            m2[i] += delta * ((float)g[i] - mean[i]);
        }
        if (norm2 < 0.81f || norm2 > 1.21f) return 0;
    }
    for (unsigned i = 0; i < 3; ++i) {
        if (m2[i] / (samples - 1u) > GYRO_SCALE_500DPS * GYRO_SCALE_500DPS)
            return 0; /* standard deviation > 1 dps */
    }
    for (unsigned i = 0; i < 3; ++i) calib->gyro_bias[i] = mean[i];
    MPU6500_ResetAcquisition();
    return 1;
}

/* MPU_SAMPLE_OK=1, NONE=0, ERROR=-1. Output stays unchanged on failure. */
int MPU6500_ReadSample_I2C(const MPU_Calib_t *calib, SensorData_t *out) {
    uint32_t stamp, sequence;
    int16_t a[3], g[3];
    float ag[3], gd[3];
    SensorData_t next;
    if (calib == NULL || out == NULL) return MPU_SAMPLE_ERROR;
    if (!take_event(&stamp, &sequence)) return MPU_SAMPLE_NONE;
    if (MPU6500_ReadRaw_I2C(a, g) || g_mpu_irq_count != sequence) {
        g_mpu_dropped++;
        return MPU_SAMPLE_ERROR;
    }
    MPU6500_GetScaled_I2C(a, g, calib, ag, gd);
    next.ax=ag[0]; next.ay=ag[1]; next.az=ag[2];
    next.gx=gd[0]; next.gy=gd[1]; next.gz=gd[2];
    next.timestamp = stamp;
    next.dt = have_previous ? (uint32_t)(stamp-last_sample_us)*1.0e-6f : 0.0f;
    last_sample_us = stamp;
    have_previous = 1;
    *out = next;
    return MPU_SAMPLE_OK;
}

/* Measure one stationary face. Repeat manually for +X,-X,+Y,-Y,+Z,-Z. */
uint8_t MPU6500_MeanAccelFace(uint16_t samples, float mean_raw[3]) {
    float mean[3] = {0}, m2[3] = {0};
    int16_t a[3], g[3];
    uint32_t stamp, sequence;
    if (samples < 2 || mean_raw == NULL) return 0;
    MPU6500_ResetAcquisition();
    for (uint32_t n=1; n<=samples; ++n) {
        uint32_t start = MPU6500_TimeUs();
        while (!take_event(&stamp, &sequence)) {
            if ((uint32_t)(MPU6500_TimeUs()-start) > 50000u) return 0;
        }
        if (MPU6500_ReadRaw_I2C(a,g) || g_mpu_irq_count != sequence) return 0;
        for (unsigned i=0; i<3; ++i) {
            float delta = a[i]-mean[i];
            mean[i] += delta/(float)n;
            m2[i] += delta*(a[i]-mean[i]);
        }
    }
    for (unsigned i=0; i<3; ++i)
        if (m2[i]/(samples-1u) > (0.03f*ACCEL_SCALE_4G)*(0.03f*ACCEL_SCALE_4G))
            return 0;
    for (unsigned i=0; i<3; ++i) mean_raw[i]=mean[i];
    MPU6500_ResetAcquisition();
    return 1;
}
