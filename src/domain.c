#include "gcu/defaults.h"
#include "gcu/domain.h"
#include "gcu/version.h"

#include <stdio.h>
#include <string.h>

/* RGB888 → RGB565. Panel is configured BGR+invert; do not also swap here
 * (double-swap made dark-blue themes read as purple on M5GO glass). */
static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

gcu_theme_colors_t gcu_theme_colors(gcu_theme_t theme) {
  gcu_theme_colors_t c;
  switch (theme) {
  case GCU_THEME_ORANGE:
    c.r = 255;
    c.g = 140;
    c.b = 0;
    c.bg565 = rgb565(40, 20, 0);
    break;
  case GCU_THEME_RED:
    c.r = 255;
    c.g = 40;
    c.b = 40;
    c.bg565 = rgb565(40, 0, 0);
    break;
  case GCU_THEME_GREEN:
    c.r = 40;
    c.g = 220;
    c.b = 80;
    c.bg565 = rgb565(0, 30, 10);
    break;
  case GCU_THEME_BLACK:
    c.r = 0;
    c.g = 0;
    c.b = 0;
    c.bg565 = rgb565(255, 255, 255);
    c.fg565 = rgb565(0, 0, 0);
    c.hair565 = rgb565(220, 220, 220);
    c.ink565 = rgb565(0, 0, 0);
    return c;
  case GCU_THEME_BLUE:
  default:
    c.r = 40;
    c.g = 140;
    c.b = 255;
    c.bg565 = rgb565(0, 16, 48);
    break;
  }
  c.fg565 = rgb565(c.r, c.g, c.b);
  c.hair565 = rgb565((uint8_t)(c.r / 3 + 10), (uint8_t)(c.g / 3 + 10),
                      (uint8_t)(c.b / 3 + 20));
  c.ink565 = rgb565(240, 240, 240);
  return c;
}

void gcu_identity_line(char *out, int out_len) {
  if (!out || out_len < 8) {
    return;
  }
  snprintf(out, (size_t)out_len, "fw_name=%s fw_version=%s", GCU_FW_NAME,
           GCU_FW_VERSION);
}

void gcu_init(gcu_state_t *st, gcu_hal_t *hal) {
  memset(st, 0, sizeof(*st));
  st->hal = hal;
  st->tick_sleep_ms = GCU_DEFAULTS.tick_sleep_ms;
  st->wink_period_ms = GCU_DEFAULTS.wink_period_ms;
  st->wink_hold_ms = GCU_DEFAULTS.wink_hold_ms;
  st->banner_step_ms = GCU_DEFAULTS.banner_step_ms;
  st->debounce_ms = GCU_DEFAULTS.debounce_ms;
  st->sample_rate_hz = GCU_DEFAULTS.sample_rate_hz;
  st->screen = GCU_SCREEN_FACE;
  st->theme = GCU_THEME_BLUE;
  st->music = GCU_MUSIC_IDLE;
  st->needs_full_paint = 1;
  st->last_wink_ms = (hal && hal->now_ms) ? hal->now_ms(hal) : 0;
  st->last_banner_ms = st->last_wink_ms;
}

void gcu_set_assets(gcu_state_t *st, const uint8_t *boot, int boot_len,
                    const uint8_t *song, int song_len, int sample_rate_hz) {
  st->boot_pcm = boot;
  st->boot_pcm_len = boot_len;
  st->song_pcm = song;
  st->song_pcm_len = song_len;
  if (sample_rate_hz > 0) {
    st->sample_rate_hz = sample_rate_hz;
  }
}

void gcu_start_boot(gcu_state_t *st) {
  if (!st->hal || !st->hal->play_pcm || !st->boot_pcm || st->boot_pcm_len <= 0) {
    st->boot_done = 1;
    return;
  }
  (void)st->hal->play_pcm(st->hal, st->boot_pcm, st->boot_pcm_len,
                          st->sample_rate_hz);
}

static int edge_press(int prev, int now) { return (!prev && now) ? 1 : 0; }

static void apply_side_leds(gcu_state_t *st) {
  if (!st->hal || !st->hal->set_side_rgb) {
    return;
  }
  gcu_theme_colors_t c = gcu_theme_colors(st->theme);
  if (st->theme == GCU_THEME_BLACK) {
    st->hal->set_side_rgb(st->hal, 0, 0, 0);
  } else {
    st->hal->set_side_rgb(st->hal, c.r, c.g, c.b);
  }
}

static void cycle_theme(gcu_state_t *st) {
  st->theme = (gcu_theme_t)(((int)st->theme + 1) % (int)GCU_THEME_COUNT);
  st->needs_full_paint = 1;
  apply_side_leds(st);
}

static void handle_buttons(gcu_state_t *st, int64_t now) {
  if (!st->hal) {
    return;
  }
  int a = st->hal->btn_a ? st->hal->btn_a(st->hal) : 0;
  int b = st->hal->btn_b ? st->hal->btn_b(st->hal) : 0;
  int c = st->hal->btn_c ? st->hal->btn_c(st->hal) : 0;

  if (now - st->last_btn_ms < (int64_t)st->debounce_ms) {
    st->prev_a = a;
    st->prev_b = b;
    st->prev_c = c;
    return;
  }

  if (edge_press(st->prev_a, a)) {
    st->last_btn_ms = now;
    if (st->screen == GCU_SCREEN_DETAILS) {
      st->screen = GCU_SCREEN_FACE;
      st->needs_full_paint = 1;
    } else if (st->music == GCU_MUSIC_PLAYING) {
      if (st->hal->audio_stop) {
        st->hal->audio_stop(st->hal);
      }
      st->music = GCU_MUSIC_PAUSED;
      st->needs_hints_paint = 1;
    } else {
      cycle_theme(st);
    }
  }

  if (edge_press(st->prev_b, b)) {
    st->last_btn_ms = now;
    if (st->music == GCU_MUSIC_PLAYING) {
      if (st->hal->audio_stop) {
        st->hal->audio_stop(st->hal);
      }
      st->music = GCU_MUSIC_PAUSED;
    } else if (st->music == GCU_MUSIC_PAUSED || st->music == GCU_MUSIC_IDLE) {
      const uint8_t *pcm = st->song_pcm;
      int len = st->song_pcm_len;
      if ((!pcm || len <= 0) && st->boot_pcm && st->boot_pcm_len > 0) {
        /* First-ship stand-in until First.pcm is product-sourced. */
        pcm = st->boot_pcm;
        len = st->boot_pcm_len;
        st->song_offset = 0;
      }
      if (pcm && len > 0 && st->hal->play_pcm) {
        int off = (st->music == GCU_MUSIC_PAUSED) ? st->song_offset : 0;
        if (off < 0 || off >= len) {
          off = 0;
        }
        if (st->hal->play_pcm(st->hal, pcm + off, len - off, st->sample_rate_hz) ==
            0) {
          st->music = GCU_MUSIC_PLAYING;
        }
      }
    }
    st->needs_hints_paint = 1;
    st->needs_full_paint = 1;
  }

  if (edge_press(st->prev_c, c)) {
    st->last_btn_ms = now;
    if (st->screen == GCU_SCREEN_FACE) {
      st->screen = GCU_SCREEN_DETAILS;
      st->needs_full_paint = 1;
    }
  }

  st->prev_a = a;
  st->prev_b = b;
  st->prev_c = c;
}

static void advance_wink_banner(gcu_state_t *st, int64_t now) {
  if (st->screen != GCU_SCREEN_FACE) {
    return;
  }
  if (st->wink_closed && now >= st->wink_until_ms) {
    st->wink_closed = 0;
    st->needs_eye_paint = 1;
  } else if (!st->wink_closed &&
             now - st->last_wink_ms >= (int64_t)st->wink_period_ms) {
    st->last_wink_ms = now;
    st->wink_closed = 1;
    st->wink_until_ms = now + (int64_t)st->wink_hold_ms;
    st->needs_eye_paint = 1;
  }

  if (now - st->last_banner_ms >= (int64_t)st->banner_step_ms) {
    st->last_banner_ms = now;
    st->banner_px += 1; /* 1px/step at ~25 Hz reads smoother than 2px jumps */
    if (st->banner_px > 360) {
      st->banner_px = 0;
    }
    st->needs_banner_paint = 1;
  }
}

void gcu_tick(gcu_state_t *st) {
  st->tick_count += 1;
  int64_t now = 0;
  if (st->hal && st->hal->now_ms) {
    now = st->hal->now_ms(st->hal);
  }

  if (!st->boot_done) {
    int busy = st->hal && st->hal->audio_busy ? st->hal->audio_busy(st->hal) : 0;
    if (!busy) {
      st->boot_done = 1;
      st->needs_full_paint = 1;
      apply_side_leds(st);
    }
  }

  if (st->music == GCU_MUSIC_PLAYING) {
    int busy = st->hal && st->hal->audio_busy ? st->hal->audio_busy(st->hal) : 0;
    if (!busy) {
      st->music = GCU_MUSIC_IDLE;
      st->song_offset = 0;
      st->needs_hints_paint = 1;
      st->needs_full_paint = 1;
    }
  }

  handle_buttons(st, now);
  advance_wink_banner(st, now);
  gcu_paint(st);
}

int gcu_tick_sleep_ms(const gcu_state_t *st) { return st->tick_sleep_ms; }

/* Minimal 5x7 glyph set (ASCII 32..90 + digits). */
static const uint8_t FONT5X7[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, /* space */
    {0x00, 0x00, 0x5F, 0x00, 0x00}, /* ! */
    {0x00, 0x07, 0x00, 0x07, 0x00}, /* " */
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, /* # */
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, /* $ */
    {0x23, 0x13, 0x08, 0x64, 0x62}, /* % */
    {0x36, 0x49, 0x55, 0x22, 0x50}, /* & */
    {0x00, 0x05, 0x03, 0x00, 0x00}, /* ' */
    {0x00, 0x1C, 0x22, 0x41, 0x00}, /* ( */
    {0x00, 0x41, 0x22, 0x1C, 0x00}, /* ) */
    {0x08, 0x2A, 0x1C, 0x2A, 0x08}, /* * */
    {0x08, 0x08, 0x3E, 0x08, 0x08}, /* + */
    {0x00, 0x50, 0x30, 0x00, 0x00}, /* , */
    {0x08, 0x08, 0x08, 0x08, 0x08}, /* - */
    {0x00, 0x60, 0x60, 0x00, 0x00}, /* . */
    {0x20, 0x10, 0x08, 0x04, 0x02}, /* / */
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, /* 0 */
    {0x00, 0x42, 0x7F, 0x40, 0x00}, /* 1 */
    {0x42, 0x61, 0x51, 0x49, 0x46}, /* 2 */
    {0x21, 0x41, 0x45, 0x4B, 0x31}, /* 3 */
    {0x18, 0x14, 0x12, 0x7F, 0x10}, /* 4 */
    {0x27, 0x45, 0x45, 0x45, 0x39}, /* 5 */
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, /* 6 */
    {0x01, 0x71, 0x09, 0x05, 0x03}, /* 7 */
    {0x36, 0x49, 0x49, 0x49, 0x36}, /* 8 */
    {0x06, 0x49, 0x49, 0x29, 0x1E}, /* 9 */
    {0x00, 0x36, 0x36, 0x00, 0x00}, /* : */
    {0x00, 0x56, 0x36, 0x00, 0x00}, /* ; */
    {0x00, 0x08, 0x14, 0x22, 0x41}, /* < */
    {0x14, 0x14, 0x14, 0x14, 0x14}, /* = */
    {0x41, 0x22, 0x14, 0x08, 0x00}, /* > */
    {0x02, 0x01, 0x51, 0x09, 0x06}, /* ? */
    {0x32, 0x49, 0x79, 0x41, 0x3E}, /* @ */
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, /* A */
    {0x7F, 0x49, 0x49, 0x49, 0x36}, /* B */
    {0x3E, 0x41, 0x41, 0x41, 0x22}, /* C */
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, /* D */
    {0x7F, 0x49, 0x49, 0x49, 0x41}, /* E */
    {0x7F, 0x09, 0x09, 0x01, 0x01}, /* F */
    {0x3E, 0x41, 0x41, 0x51, 0x32}, /* G */
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, /* H */
    {0x00, 0x41, 0x7F, 0x41, 0x00}, /* I */
    {0x20, 0x40, 0x41, 0x3F, 0x01}, /* J */
    {0x7F, 0x08, 0x14, 0x22, 0x41}, /* K */
    {0x7F, 0x40, 0x40, 0x40, 0x40}, /* L */
    {0x7F, 0x02, 0x04, 0x02, 0x7F}, /* M */
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, /* N */
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, /* O */
    {0x7F, 0x09, 0x09, 0x09, 0x06}, /* P */
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, /* Q */
    {0x7F, 0x09, 0x19, 0x29, 0x46}, /* R */
    {0x46, 0x49, 0x49, 0x49, 0x31}, /* S */
    {0x01, 0x01, 0x7F, 0x01, 0x01}, /* T */
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, /* U */
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, /* V */
    {0x7F, 0x20, 0x18, 0x20, 0x7F}, /* W */
    {0x63, 0x14, 0x08, 0x14, 0x63}, /* X */
    {0x03, 0x04, 0x78, 0x04, 0x03}, /* Y */
    {0x61, 0x51, 0x49, 0x45, 0x43}, /* Z */
};

static void draw_char(gcu_hal_t *hal, int x, int y, char ch, uint16_t fg,
                      uint16_t bg, int scale) {
  if (!hal || !hal->fill_rect) {
    return;
  }
  unsigned char c = (unsigned char)ch;
  if (c >= 'a' && c <= 'z') {
    c = (unsigned char)(c - 'a' + 'A');
  }
  int idx = 0;
  if (c >= 32 && c <= 90) {
    idx = (int)(c - 32);
  }
  for (int col = 0; col < 5; col++) {
    uint8_t bits = FONT5X7[idx][col];
    for (int row = 0; row < 7; row++) {
      uint16_t color = (bits & (1u << row)) ? fg : bg;
      hal->fill_rect(hal, x + col * scale, y + row * scale, scale, scale, color);
    }
  }
}

static void draw_text(gcu_hal_t *hal, int x, int y, const char *s, uint16_t fg,
                      uint16_t bg, int scale) {
  int cx = x;
  for (; s && *s; s++) {
    draw_char(hal, cx, y, *s, fg, bg, scale);
    cx += 6 * scale;
  }
}

/* Glyph hints (not text labels): palette / play-pause / gear. */
static void paint_button_glyphs(gcu_hal_t *hal, gcu_state_t *st,
                                const gcu_theme_colors_t *c) {
  if (!hal || !hal->fill_rect || !st || !c) {
    return;
  }
  /* Clear hint strip */
  hal->fill_rect(hal, 0, 205, GCU_LCD_W, 35, c->bg565);

  /* Left: color swatches (3 mini blocks). */
  int lx = 28;
  int ly = 214;
  uint16_t sw[] = {rgb565(40, 140, 255), rgb565(255, 140, 0), rgb565(40, 220, 80)};
  for (int i = 0; i < 3; i++) {
    hal->fill_rect(hal, lx + i * 14, ly, 12, 12, sw[i]);
  }

  /* Middle: play triangle or pause bars. */
  int mx = 150;
  int my = 212;
  if (st->music == GCU_MUSIC_PLAYING) {
    hal->fill_rect(hal, mx, my, 6, 18, c->ink565);
    hal->fill_rect(hal, mx + 12, my, 6, 18, c->ink565);
  } else {
    for (int row = 0; row < 18; row++) {
      int w = 2 + row / 2;
      if (w > 12) {
        w = 12;
      }
      hal->fill_rect(hal, mx, my + row, w, 1, c->ink565);
    }
  }

  /* Right: simple gear (hub + teeth). */
  int gx = 268;
  int gy = 218;
  hal->fill_rect(hal, gx + 6, gy, 8, 16, c->ink565);
  hal->fill_rect(hal, gx, gy + 6, 20, 8, c->ink565);
  hal->fill_rect(hal, gx + 8, gy + 8, 4, 4, c->bg565);
}

static void paint_banner(gcu_state_t *st, const gcu_theme_colors_t *c) {
  gcu_hal_t *hal = st->hal;
  if (!hal || !hal->fill_rect || !c) {
    return;
  }
  /* Clear hair bar then draw text once per step (scale 1 = less SPI thrash). */
  hal->fill_rect(hal, 0, 0, GCU_LCD_W, 28, c->hair565);
  draw_text(hal, GCU_LCD_W - st->banner_px, 10, GCU_BANNER_TEXT, c->ink565,
            c->hair565, 1);
  /* Second copy for seamless loop. */
  draw_text(hal, GCU_LCD_W - st->banner_px + 200, 10, GCU_BANNER_TEXT, c->ink565,
            c->hair565, 1);
}

static void paint_eye(gcu_hal_t *hal, int cx, int cy, int closed, uint16_t fg,
                      uint16_t bg) {
  if (!hal || !hal->fill_rect) {
    return;
  }
  /* Clear eye region then draw open circle-ish or closed line. */
  hal->fill_rect(hal, cx - 18, cy - 14, 36, 28, bg);
  if (closed) {
    hal->fill_rect(hal, cx - 14, cy - 2, 28, 4, fg);
  } else {
    hal->fill_rect(hal, cx - 12, cy - 12, 24, 24, fg);
    hal->fill_rect(hal, cx - 5, cy - 5, 10, 10, bg);
  }
}

static void paint_face_full(gcu_state_t *st) {
  gcu_hal_t *hal = st->hal;
  if (!hal || !hal->fill_rect) {
    return;
  }
  gcu_theme_colors_t c = gcu_theme_colors(st->theme);
  hal->fill_rect(hal, 0, 0, GCU_LCD_W, GCU_LCD_H, c.bg565);

  /* Hair bar + banner (scale 1 for denser scroll; soft-step in tick). */
  paint_banner(st, &c);

  /* Eyes */
  paint_eye(hal, 110, 95, 0, c.fg565, c.bg565);
  paint_eye(hal, 210, 95, st->wink_closed, c.fg565, c.bg565);

  /* Smile (U shape: side posts rise from a lower bar). */
  hal->fill_rect(hal, 100, 168, 120, 8, c.fg565);
  hal->fill_rect(hal, 100, 150, 8, 26, c.fg565);
  hal->fill_rect(hal, 212, 150, 8, 26, c.fg565);

  /* Playing cue */
  if (st->music == GCU_MUSIC_PLAYING) {
    draw_text(hal, 60, 185, "First by Tig", c.ink565, c.bg565, 2);
  }

  paint_button_glyphs(hal, st, &c);
}

static void paint_details(gcu_state_t *st) {
  gcu_hal_t *hal = st->hal;
  if (!hal || !hal->fill_rect) {
    return;
  }
  gcu_theme_colors_t c = gcu_theme_colors(st->theme);
  hal->fill_rect(hal, 0, 0, GCU_LCD_W, GCU_LCD_H, c.bg565);
  char line[48];
  snprintf(line, sizeof line, "%s %s", GCU_FW_NAME, GCU_FW_VERSION);
  draw_text(hal, 12, 20, line, c.ink565, c.bg565, 2);
  draw_text(hal, 12, 60, "Details", c.fg565, c.bg565, 2);
  draw_text(hal, 12, 100, "IMU live next", c.ink565, c.bg565, 1);
  draw_text(hal, 12, 130, "A: back to face", c.ink565, c.bg565, 1);
  if (st->music == GCU_MUSIC_PLAYING) {
    draw_text(hal, 12, 160, "Music playing", c.ink565, c.bg565, 1);
  }
}

void gcu_paint(gcu_state_t *st) {
  if (!st->hal || !st->hal->fill_rect) {
    st->needs_full_paint = 0;
    st->needs_eye_paint = 0;
    st->needs_banner_paint = 0;
    st->needs_hints_paint = 0;
    return;
  }

  if (st->needs_full_paint) {
    if (st->screen == GCU_SCREEN_DETAILS) {
      paint_details(st);
    } else {
      paint_face_full(st);
    }
    st->needs_full_paint = 0;
    st->needs_eye_paint = 0;
    st->needs_banner_paint = 0;
    st->needs_hints_paint = 0;
    return;
  }

  if (st->screen != GCU_SCREEN_FACE) {
    return;
  }

  gcu_theme_colors_t c = gcu_theme_colors(st->theme);
  if (st->needs_banner_paint) {
    paint_banner(st, &c);
    st->needs_banner_paint = 0;
  }
  if (st->needs_eye_paint) {
    paint_eye(st->hal, 210, 95, st->wink_closed, c.fg565, c.bg565);
    st->needs_eye_paint = 0;
  }
  if (st->needs_hints_paint) {
    paint_button_glyphs(st->hal, st, &c);
    st->needs_hints_paint = 0;
  }
}
