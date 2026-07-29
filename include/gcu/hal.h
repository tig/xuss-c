#ifndef GCU_HAL_H
#define GCU_HAL_H

/* Portable HAL contract — no device headers here. */

#include <stdint.h>

typedef struct gcu_hal gcu_hal_t;

struct gcu_hal {
  /* Legacy plate LED (unused when set_side_rgb is present). */
  void (*set_led)(gcu_hal_t *self, int on);
  /* Side strip solid color (0,0,0 = off). */
  void (*set_side_rgb)(gcu_hal_t *self, uint8_t r, uint8_t g, uint8_t b);
  void (*delay_ms)(gcu_hal_t *self, int ms);
  /* Monotonic ms since boot. MUST be int64_t (ESP32 long is 32-bit). */
  int64_t (*now_ms)(gcu_hal_t *self);
  /* RGB565 fill of axis-aligned rect. */
  void (*fill_rect)(gcu_hal_t *self, int x, int y, int w, int h, uint16_t rgb565);
  /* RGB565 packed row-major blit (native host words; display converts to SPI BE). */
  void (*blit)(gcu_hal_t *self, int x, int y, int w, int h, const uint16_t *pixels);
  /* Active-low buttons: 1 = pressed. */
  int (*btn_a)(gcu_hal_t *self);
  int (*btn_b)(gcu_hal_t *self);
  int (*btn_c)(gcu_hal_t *self);
  /* Start non-blocking PCM (u8 mono). Returns 0 ok, non-zero if busy/missing. */
  int (*play_pcm)(gcu_hal_t *self, const uint8_t *data, int len, int sample_rate);
  int (*audio_busy)(gcu_hal_t *self);
  void (*audio_stop)(gcu_hal_t *self);
};

#endif
