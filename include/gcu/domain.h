#ifndef GCU_DOMAIN_H
#define GCU_DOMAIN_H

#include "gcu/defaults.h"
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
  GCU_BTN_NONE = 0,
  GCU_BTN_A = 1,
  GCU_BTN_B = 2,
  GCU_BTN_C = 3,
} gcu_btn_t;

/* Output requests domain makes of the board after a tick (portable). */
typedef struct {
  int theme_index;
  gcu_screen_t screen;
  gcu_music_t music;
  int wink_closed;          /* right eye closed */
  int banner_offset_px;     /* hair scroll offset */
  int full_repaint;         /* theme/screen change */
  int eye_repaint;          /* wink edge only */
  int banner_repaint;       /* banner strip only */
  int request_play;         /* start or resume full song */
  int request_pause;        /* pause full song */
  int request_boot_riff;    /* play boot riff once (set at init) */
  int sides_rgb_r;
  int sides_rgb_g;
  int sides_rgb_b;
  int sides_on;
} gcu_view_t;

typedef struct {
  gcu_hal_t *hal;
  int theme_index;
  gcu_screen_t screen;
  gcu_music_t music;
  int64_t boot_ms;
  int64_t last_wink_ms;
  int wink_closed;
  int64_t wink_close_until_ms;
  int banner_offset_px;
  int64_t last_banner_ms;
  int64_t music_offset_bytes;
  int boot_riff_requested;
  int full_repaint;
  int eye_repaint;
  int banner_repaint;
  int request_play;
  int request_pause;
  /* button edge latch (debounced outside or via gcu_on_button) */
} gcu_state_t;

void gcu_identity_line(char *out, int out_len);
void gcu_init(gcu_state_t *st, gcu_hal_t *hal);
/* Wall-clock UI/service tick. Clears one-shot request flags after packing view. */
void gcu_tick(gcu_state_t *st, gcu_view_t *out);
/* Debounced rising edge (press). Returns 1 if handled. */
int gcu_on_button(gcu_state_t *st, gcu_btn_t btn);
/* Music engine reports natural end of full track. */
void gcu_on_music_ended(gcu_state_t *st);
/* Music engine reports pause completed / offset. */
void gcu_set_music_offset(gcu_state_t *st, int64_t offset_bytes);
int gcu_theme_count(void);
const gcu_theme_t *gcu_theme_at(int index);

#endif
