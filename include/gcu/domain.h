#ifndef GCU_DOMAIN_H
#define GCU_DOMAIN_H

#include "gcu/hal.h"

#include <stdint.h>

typedef enum {
  GCU_SCREEN_FACE = 0,
  GCU_SCREEN_DETAILS = 1,
} gcu_screen_t;

typedef enum {
  GCU_MUSIC_IDLE = 0,
  GCU_MUSIC_PLAYING = 1,
  GCU_MUSIC_PAUSED = 2,
} gcu_music_t;

typedef enum {
  GCU_THEME_BLUE = 0,
  GCU_THEME_ORANGE = 1,
  GCU_THEME_RED = 2,
  GCU_THEME_GREEN = 3,
  GCU_THEME_BLACK = 4,
  GCU_THEME_COUNT = 5,
} gcu_theme_t;

typedef struct {
  uint8_t r, g, b;
  uint16_t bg565;
  uint16_t fg565;
  uint16_t hair565;
  uint16_t ink565;
} gcu_theme_colors_t;

typedef struct {
  gcu_hal_t *hal;
  int tick_count;
  int tick_sleep_ms;
  int wink_period_ms;
  int wink_hold_ms;
  int banner_step_ms;
  int debounce_ms;

  gcu_screen_t screen;
  gcu_theme_t theme;
  gcu_music_t music;
  int wink_closed;
  int banner_px;
  int needs_full_paint;
  int needs_eye_paint;
  int needs_banner_paint;
  int needs_hints_paint;
  int needs_details_values;

  int64_t last_wink_ms;
  int64_t wink_until_ms;
  int64_t last_banner_ms;
  int64_t last_btn_ms;
  int64_t last_details_ms;
  int prev_a, prev_b, prev_c;
  gcu_sensors_t sensors;

  /* Boot / song assets (metal fills pointers; host tests may leave NULL). */
  const uint8_t *boot_pcm;
  int boot_pcm_len;
  const uint8_t *song_pcm;
  int song_pcm_len;
  int sample_rate_hz;
  int song_sample_rate_hz;
  int song_offset;
  int boot_done;
} gcu_state_t;

void gcu_identity_line(char *out, int out_len);
void gcu_init(gcu_state_t *st, gcu_hal_t *hal);
void gcu_set_assets(gcu_state_t *st, const uint8_t *boot, int boot_len,
                    const uint8_t *song, int song_len, int sample_rate_hz);
void gcu_start_boot(gcu_state_t *st);
void gcu_tick(gcu_state_t *st);
int gcu_tick_sleep_ms(const gcu_state_t *st);
gcu_theme_colors_t gcu_theme_colors(gcu_theme_t theme);
void gcu_paint(gcu_state_t *st);

#endif
