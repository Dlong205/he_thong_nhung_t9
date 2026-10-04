#include "mpu6500_i2c.h"
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
            /* SV3: use sensor_i2c only here. First dt=0: initialize filter.
             * On a large dt gap, reset/handle discontinuity in the filter.
             * No filter or blocking UART work is performed in the ISR.
             */
        } else if (result == MPU_SAMPLE_ERROR) {
            read_errors++;
        }
        if ((uint32_t)(MPU6500_TimeUs()-last_ok) > 100000u)
            no_data_fault = 1;
    }
}
