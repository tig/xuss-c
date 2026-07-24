#ifndef GCU_DETAILS_H
#define GCU_DETAILS_H

/* Details screen data model + glyph-safe numeric formatting (§4.5).
 * Digits must render readable, never solid blocks — formatting stays within
 * the required glyph set (space, 0-9, '+', '-', '.'). */

typedef struct {
  long accel_mg[3];  /* acceleration, milli-g */
  long gyro_mdps[3]; /* rotation rate, milli-deg/s */
  long imu_temp_mc;  /* IMU temperature, milli-Celsius */
  int button[3];     /* A/B/C currently held (0/1) */
  long heap_free;    /* free heap bytes, if cheap to obtain */
} gcu_details_t;

/* Format a milli-unit value with one fractional digit, e.g.
 *   1234 -> "1.2", -50 -> "-0.0", 0 -> "0.0".
 * Returns the string length written, or 0 on bad buffer. */
int gcu_fmt_milli1(char *out, int out_len, long milli);

#endif
