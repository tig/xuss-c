#ifndef GCU_HAL_H
#define GCU_HAL_H

/* Portable HAL contract — no device headers here. */

typedef struct gcu_hal gcu_hal_t;

struct gcu_hal {
  void (*set_led)(gcu_hal_t *self, int on);
  void (*delay_ms)(gcu_hal_t *self, int ms);
  /* Monotonic wall-clock milliseconds for time-based animation (§5.2). */
  long (*now_ms)(gcu_hal_t *self);
  /* Play unsigned 8-bit mono PCM to the speaker, blocking until done.
   * NULL on backends without audio (e.g. host doubles). */
  void (*play_pcm)(gcu_hal_t *self, const unsigned char *pcm, int len,
                   int sample_rate_hz);
};

#endif
