#ifndef GCU_HAL_IMU_H
#define GCU_HAL_IMU_H

#include "gcu/hal.h"

/* MPU6886 6-axis IMU on the M5GO internal I2C bus (SDA=21, SCL=22, addr 0x68).
 * gcu_imu_init() brings up the bus + sensor; gcu_imu_read() fills milli-units
 * (accel milli-g, gyro milli-deg/s, temp milli-C), returning 1 on success. */
void gcu_imu_init(void);
int gcu_imu_read(gcu_hal_t *self, long accel_mg[3], long gyro_mdps[3],
                 long *temp_mc);

#endif
