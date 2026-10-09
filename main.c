#include "mpu6500_i2c.h"
#include <stdio.h>
#include <stdint.h>

/* PA9 (USART1 TX) -> USB-UART RX, GND -> GND; 230400 baud, 8N1.
 * Send one RAW record per 10 successful sensor samples (~50 Hz nominal).
 * sensor_i2c.timestamp comes from the driver's TIM2 clock in microseconds.
 */
#define TELEMETRY_BAUD 230400u
#define TELEMETRY_INTERVAL 10u
#define TELEMETRY_TX_SIZE 512u
#define TELEMETRY_TX_MASK (TELEMETRY_TX_SIZE - 1u)

static uint8_t telemetry_tx[TELEMETRY_TX_SIZE];
static uint16_t telemetry_head, telemetry_tail;
volatile uint32_t g_telemetry_dropped;

static void Telemetry_USART1_Init(void) {
    uint32_t pclk2, ppre2;
    SystemCoreClockUpdate();
    pclk2 = SystemCoreClock;
    ppre2 = (RCC->CFGR >> 11) & 7u;
    if (ppre2 >= 4u) pclk2 >>= (ppre2 - 3u);
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_USART1EN;
    GPIOA->CRH = (GPIOA->CRH & ~(0xFu << 4)) | (0xBu << 4);
    USART1->BRR = (pclk2 + TELEMETRY_BAUD / 2u) / TELEMETRY_BAUD;
    USART1->CR1 = USART_CR1_TE | USART_CR1_UE;
}

static void Telemetry_Poll(void) {
    if (telemetry_head != telemetry_tail && (USART1->SR & USART_SR_TXE)) {
        USART1->DR = telemetry_tx[telemetry_tail];
        telemetry_tail = (uint16_t)((telemetry_tail + 1u) & TELEMETRY_TX_MASK);
    }
}

static void Telemetry_Queue(const char *data, uint16_t length) {
    uint16_t used = (uint16_t)((telemetry_head - telemetry_tail) & TELEMETRY_TX_MASK);
    uint16_t i;
    if (length > TELEMETRY_TX_MASK - used) {
        ++g_telemetry_dropped;
        return;
    }
    for (i = 0; i < length; ++i) {
        telemetry_tx[telemetry_head] = (uint8_t)data[i];
        telemetry_head = (uint16_t)((telemetry_head + 1u) & TELEMETRY_TX_MASK);
    }
}

/* Three decimal places without requiring floating point printf in Keil. */
static int Telemetry_AppendFixed3(char *buffer, uint16_t capacity, float value) {
    int32_t scaled = (int32_t)(value * 1000.0f + (value >= 0.0f ? 0.5f : -0.5f));
    uint32_t magnitude = scaled < 0 ? (uint32_t)(-(int64_t)scaled) : (uint32_t)scaled;
    return snprintf(buffer, capacity, ",%s%lu.%03lu", scaled < 0 ? "-" : "",
                    (unsigned long)(magnitude / 1000u),
                    (unsigned long)(magnitude % 1000u));
}

static void Telemetry_QueueRaw(const SensorData_t *sample) {
    char line[160];
    float values[6];
    uint16_t length, i;
    int written;
    values[0] = sample->ax; values[1] = sample->ay; values[2] = sample->az;
    values[3] = sample->gx; values[4] = sample->gy; values[5] = sample->gz;
    written = snprintf(line, sizeof(line), "RAW,%lu",
                       (unsigned long)(sample->timestamp / 1000u));
    if (written < 0 || written >= (int)sizeof(line)) {
        ++g_telemetry_dropped;
        return;
    }
    length = (uint16_t)written;
    for (i = 0; i < 6u; ++i) {
        written = Telemetry_AppendFixed3(line + length,
                                         (uint16_t)(sizeof(line) - length), values[i]);
        if (written < 0 || written >= (int)(sizeof(line) - length)) {
            ++g_telemetry_dropped;
            return;
        }
        length = (uint16_t)(length + (uint16_t)written);
    }
    if (length + 2u > sizeof(line)) {
        ++g_telemetry_dropped;
        return;
    }
    line[length++] = '\r';
    line[length++] = '\n';
    Telemetry_Queue(line, length);
}

#define SV2_CAPTURE_ACCEL_FACE 0
float face_mean_raw[3];

MPU_Calib_t calib_i2c;
SensorData_t sensor_i2c;
/* Watch: 1=running, -1=clock wrong, -2=MPU init failed, -3=gyro calib failed. */
volatile int app_status;
volatile uint32_t sample_count, read_errors, no_data_fault;
volatile float sample_dt;
volatile float acc_g_i2c[3], gyro_dps_i2c[3];

/* Define this handler ONCE in project; merge with stm32f10x_it.c if needed. */
void EXTI0_IRQHandler(void) {
    if (EXTI->PR & EXTI_PR_PR0) {
        EXTI->PR = EXTI_PR_PR0;
        MPU6500_OnDataReady();
    }
}

int main(void) {
    uint32_t last_ok;
    /* Startup's SystemInit() must already set HCLK=72 MHz, APB1=36 MHz. */
    if (!MPU6500_I2C_LowLevel_Init()) {
        app_status = -1;
        while (1) {}
    }
    Telemetry_USART1_Init();
    MPU6500_CalibDefault(&calib_i2c);
    if (!MPU6500_Init_I2C()) {
        app_status = -2;
        while (1) {}
    }
#if SV2_CAPTURE_ACCEL_FACE
    delay_ms(2000); /* let board settle after reset */
    app_status = MPU6500_MeanAccelFace(500, face_mean_raw) ? 2 : -4;
    while (1) {} /* Watch face_mean_raw, repeat for the other 5 faces. */
#endif
    /* No invented accelerometer calibration numbers.
     * After measuring all 6 stationary face means, call
     * MPU6500_CalibAccel6Face_I2C(&calib_i2c, ...six measured values...);
     */
    if (!MPU6500_CalibGyro_I2C(&calib_i2c, 500)) {
        app_status = -3;
        while (1) {}
    }
    app_status = 1;
    last_ok = MPU6500_TimeUs();
    while (1) {
        int result = MPU6500_ReadSample_I2C(&calib_i2c, &sensor_i2c);
        if (result == MPU_SAMPLE_OK) {
            sample_count++;
            sample_dt = sensor_i2c.dt;
            acc_g_i2c[0]=sensor_i2c.ax;
            acc_g_i2c[1]=sensor_i2c.ay;
            acc_g_i2c[2]=sensor_i2c.az;
            gyro_dps_i2c[0]=sensor_i2c.gx;
            gyro_dps_i2c[1]=sensor_i2c.gy;
            gyro_dps_i2c[2]=sensor_i2c.gz;
            last_ok = MPU6500_TimeUs();
            no_data_fault = 0;
            if (sample_count % TELEMETRY_INTERVAL == 0u)
                Telemetry_QueueRaw(&sensor_i2c);
            /* SV3: use sensor_i2c only here. First dt=0: initialize filter.
             * On a large dt gap, reset/handle discontinuity in the filter.
             * No filter or blocking UART work is performed in the ISR.
             */
        } else if (result == MPU_SAMPLE_ERROR) {
            read_errors++;
        }
        if ((uint32_t)(MPU6500_TimeUs()-last_ok) > 100000u)
            no_data_fault = 1;
        Telemetry_Poll();
    }
}
