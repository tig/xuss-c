#include "gcu/defaults.h"
#include "gcu/domain.h"
#include "gcu/version.h"

#include <stdio.h>
#include <string.h>

void gcu_identity_line(char *out, int out_len) {
  if (!out || out_len < 8) {
    return;
  }
  snprintf(out, (size_t)out_len, "fw_name=%s fw_version=%s", GCU_FW_NAME,
           GCU_FW_VERSION);
}

int gcu_theme_count(void) { return GCU_THEME_COUNT; }

const gcu_theme_t *gcu_theme_at(int index) {
  if (index < 0 || index >= GCU_THEME_COUNT) {
    return &GCU_THEMES[0];
  }
  return &GCU_THEMES[index];
}

void gcu_init(gcu_state_t *st, gcu_hal_t *hal) {
  memset(st, 0, sizeof(*st));
  st->hal = hal;
  st->theme_index = 0;
  st->screen = GCU_SCREEN_FACE;
  st->music = GCU_MUSIC_IDLE;
  st->boot_ms = (hal && hal->now_ms) ? hal->now_ms(hal) : 0;
  st->last_wink_ms = st->boot_ms;
  st->last_banner_ms = st->boot_ms;
  st->boot_riff_requested = 1;
  st->full_repaint = 1;
}

static void pack_theme_sides(gcu_state_t *st, gcu_view_t *out) {
  const gcu_theme_t *th = gcu_theme_at(st->theme_index);
  out->theme_index = st->theme_index;
  out->sides_on = th->sides_on;
  out->sides_rgb_r = th->face.r;
  out->sides_rgb_g = th->face.g;
  out->sides_rgb_b = th->face.b;
}

void gcu_tick(gcu_state_t *st, gcu_view_t *out) {
  memset(out, 0, sizeof(*out));
  if (!st) {
    return;
  }

  int64_t now = 0;
  if (st->hal && st->hal->now_ms) {
    now = st->hal->now_ms(st->hal);
  }

  /* Banner scroll on face (and while playing). Details freezes banner.
   * Spec: hair scrolls right → left (text x decreases each step). */
  if (st->screen == GCU_SCREEN_FACE) {
    if (now - st->last_banner_ms >= (int64_t)GCU_DEFAULTS.banner_step_ms) {
      st->last_banner_ms = now;
      /* scale-2 5x7: 6 px advance * 2 = 12 px per character on metal. */
      const int text_w = (int)strlen(GCU_BANNER_TEXT) * 12;
      st->banner_offset_px -= 1;
      if (st->banner_offset_px + text_w < 0) {
        st->banner_offset_px = GCU_DEFAULTS.display_w;
      }
      st->banner_repaint = 1;
    }

    /* Time-based wink: close right eye briefly every wink_period. */
    if (!st->wink_closed) {
      if (now - st->last_wink_ms >= (int64_t)GCU_DEFAULTS.wink_period_ms) {
        st->wink_closed = 1;
        st->wink_close_until_ms = now + (int64_t)GCU_DEFAULTS.wink_hold_ms;
        st->last_wink_ms = now;
        st->eye_repaint = 1;
      }
    } else if (now >= st->wink_close_until_ms) {
      st->wink_closed = 0;
      st->eye_repaint = 1;
    }
  }

  out->screen = st->screen;
  out->music = st->music;
  out->wink_closed = st->wink_closed;
  out->banner_offset_px = st->banner_offset_px;
  out->full_repaint = st->full_repaint;
  out->eye_repaint = st->eye_repaint;
  out->banner_repaint = st->banner_repaint;
  out->request_play = st->request_play;
  out->request_pause = st->request_pause;
  out->request_boot_riff = st->boot_riff_requested;
  pack_theme_sides(st, out);

  /* Clear one-shots after packing. */
  st->full_repaint = 0;
  st->eye_repaint = 0;
  st->banner_repaint = 0;
  st->request_play = 0;
  st->request_pause = 0;
  st->boot_riff_requested = 0;
}

int gcu_on_button(gcu_state_t *st, gcu_btn_t btn) {
  if (!st || btn == GCU_BTN_NONE) {
    return 0;
  }

  if (btn == GCU_BTN_A) {
    if (st->screen == GCU_SCREEN_DETAILS) {
      st->screen = GCU_SCREEN_FACE;
      st->full_repaint = 1;
      return 1;
    }
    if (st->music == GCU_MUSIC_PLAYING) {
      st->music = GCU_MUSIC_PAUSED;
      st->request_pause = 1;
      st->full_repaint = 1; /* clear playing cue */
      return 1;
    }
    /* Face idle/paused: next theme */
    st->theme_index = (st->theme_index + 1) % GCU_THEME_COUNT;
    st->full_repaint = 1;
    return 1;
  }

  if (btn == GCU_BTN_B) {
    if (st->music == GCU_MUSIC_PLAYING) {
      st->music = GCU_MUSIC_PAUSED;
      st->request_pause = 1;
      st->full_repaint = 1;
      return 1;
    }
    /* idle or paused → play/resume */
    st->music = GCU_MUSIC_PLAYING;
    st->request_play = 1;
    st->full_repaint = 1;
    return 1;
  }

  if (btn == GCU_BTN_C) {
    if (st->screen == GCU_SCREEN_DETAILS) {
      return 1; /* no-op */
    }
    st->screen = GCU_SCREEN_DETAILS;
    st->full_repaint = 1;
    return 1;
  }

  return 0;
}

void gcu_on_music_ended(gcu_state_t *st) {
  if (!st) {
    return;
  }
  st->music = GCU_MUSIC_IDLE;
  st->music_offset_bytes = 0;
  st->full_repaint = 1;
}

void gcu_set_music_offset(gcu_state_t *st, int64_t offset_bytes) {
  if (!st) {
    return;
  }
  st->music_offset_bytes = offset_bytes;
}
