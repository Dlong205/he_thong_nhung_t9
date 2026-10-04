#include "mpu6500_i2c.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static void near(float x,float y) {assert(fabsf(x-y)<0.0001f);}
int main(void) {
    MPU_Calib_t c, before;
    int16_t a[3]={8192,0,-8192}, g[3]={131,-131,0};
    float ag[3], gd[3];
    MPU6500_CalibDefault(&c);
    MPU6500_GetScaled_I2C(a,g,&c,ag,gd);
    near(ag[0],1); near(ag[1],0); near(ag[2],-1);
    near(gd[0],2); near(gd[1],-2); near(gd[2],0);
    assert(MPU6500_CalibAccel6Face_I2C(&c,8300,-8100,8200,-8000,8400,-8200));
    for(unsigned i=0;i<3;i++) near(c.acc_offset[i],100);
    int16_t plus[3]={8300,8200,8400}, minus[3]={-8100,-8000,-8200};
    MPU6500_GetScaled_I2C(plus,g,&c,ag,gd);
    for(unsigned i=0;i<3;i++) near(ag[i],1);
    MPU6500_GetScaled_I2C(minus,g,&c,ag,gd);
    for(unsigned i=0;i<3;i++) near(ag[i],-1);
    c.gyro_bias[0]=131; c.gyro_bias[1]=-131;
    MPU6500_GetScaled_I2C(a,g,&c,ag,gd);
    for(unsigned i=0;i<3;i++) near(gd[i],0);
    before=c;
    assert(!MPU6500_CalibAccel6Face_I2C(&c,0,0,8192,-8192,8192,-8192));
    assert(!memcmp(&c,&before,sizeof c));
    assert(!MPU6500_CalibAccel6Face_I2C(&c,8192,-8192,NAN,-8192,8192,-8192));
    assert(!memcmp(&c,&before,sizeof c));
    puts("PASS: C99 syntax (mock CMSIS), default scale, signed units, six-face correction, gyro bias subtraction, invalid calibration preserves previous values.");
    return 0;
}
