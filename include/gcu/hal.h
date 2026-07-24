#ifndef GCU_HAL_H
#define GCU_HAL_H

/* Portable HAL contract — no device headers here. */

typedef struct gcu_hal gcu_hal_t;

struct gcu_hal {
  void (*set_led)(gcu_hal_t *self, int on);
  void (*delay_ms)(gcu_hal_t *self, int ms);
  /* Monotonic wall-clock milliseconds for time-based animation (§5.2). */
  long (*now_ms)(gcu_hal_t *self);
  /* Read the front-button pressed mask (bit0=A, bit1=B, bit2=C; 1=pressed). */
  int (*read_buttons)(gcu_hal_t *self);
  /* Sample the IMU into milli-units (accel milli-g, gyro milli-deg/s, temp
   * milli-C). Returns 1 on success, 0 if unavailable. */
  int (*read_imu)(gcu_hal_t *self, long accel_mg[3], long gyro_mdps[3],
                  long *temp_mc);
  /* Free heap in bytes, or -1 if not tracked. */
  long (*free_heap)(gcu_hal_t *self);
  /* Set all side LEDs to one RGB color (0,0,0 = off). NULL if no strip. */
  void (*set_leds)(gcu_hal_t *self, int r, int g, int b);
  /* Play unsigned 8-bit mono PCM to the speaker, blocking until done.
   * NULL on backends without audio (e.g. host doubles). */
  void (*play_pcm)(gcu_hal_t *self, const unsigned char *pcm, int len,
                   int sample_rate_hz);
};

#endif
