#ifndef GCU_HAL_H
#define GCU_HAL_H

/* Portable HAL contract — no device headers here. */

#include <stdint.h>

typedef struct gcu_hal gcu_hal_t;

typedef struct {
  int present;
  float ax, ay, az;
  float gx, gy, gz;
  float temp_c;
  int btn_a, btn_b, btn_c;
  int heap_free;
} gcu_sensors_t;

struct gcu_hal {
  void (*set_led)(gcu_hal_t *self, int on);
  void (*set_side_rgb)(gcu_hal_t *self, uint8_t r, uint8_t g, uint8_t b);
  void (*delay_ms)(gcu_hal_t *self, int ms);
  int64_t (*now_ms)(gcu_hal_t *self);
  void (*fill_rect)(gcu_hal_t *self, int x, int y, int w, int h, uint16_t rgb565);
  void (*blit)(gcu_hal_t *self, int x, int y, int w, int h, const uint16_t *pixels);
  int (*btn_a)(gcu_hal_t *self);
  int (*btn_b)(gcu_hal_t *self);
  int (*btn_c)(gcu_hal_t *self);
  int (*play_pcm)(gcu_hal_t *self, const uint8_t *data, int len, int sample_rate);
  /* Stream full track from path; start_offset for resume. 0=ok. */
  int (*play_file)(gcu_hal_t *self, const char *path, int sample_rate,
                   int start_offset);
  int (*audio_busy)(gcu_hal_t *self);
  int (*audio_position)(gcu_hal_t *self);
  void (*audio_stop)(gcu_hal_t *self);
  int (*read_sensors)(gcu_hal_t *self, gcu_sensors_t *out);
  /* Non-zero size means full track is available. */
  int (*song_size)(gcu_hal_t *self);
  const char *(*song_path)(gcu_hal_t *self);
};

#endif
