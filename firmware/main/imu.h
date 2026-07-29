#ifndef XUSSC_IMU_H
#define XUSSC_IMU_H

#include <stdint.h>

typedef struct {
  int present;
  float ax, ay, az;   /* g */
  float gx, gy, gz;   /* deg/s */
  float temp_c;
} imu_sample_t;

int imu_init(void);
int imu_read(imu_sample_t *out);

#endif
