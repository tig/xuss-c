#ifndef GCU_SENSOR_BOARD_H
#define GCU_SENSOR_BOARD_H

/* Returns 1 when the MPU6886 answers and is configured; prints imu=ok /
 * imu=missing either way. Details stays usable with zeros when missing. */
int gcu_board_imu_init(void);

/* One burst sample: accel milli-g, gyro deg/s, temp deci-degC.
 * Returns 0 on read failure (values untouched). */
int gcu_board_imu_read(int mg[3], int dps[3], int *temp_dc);

#endif
