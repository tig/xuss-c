#include "gcu/defaults.h"
#include "gcu/domain.h"
#include "gcu/version.h"

#include <stdio.h>
#include <string.h>
/* memmove for banner scroll */
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
  st->song_sample_rate_hz = GCU_DEFAULTS.song_sample_rate_hz;
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
      if (st->hal->audio_position) {
        st->song_offset = st->hal->audio_position(st->hal);
      }
      if (st->hal->audio_stop) {
        st->hal->audio_stop(st->hal);
      }
      st->music = GCU_MUSIC_PAUSED;
      st->needs_hints_paint = 1;
      st->needs_full_paint = 1;
    } else {
      cycle_theme(st);
    }
  }

  if (edge_press(st->prev_b, b)) {
    st->last_btn_ms = now;
    if (st->music == GCU_MUSIC_PLAYING) {
      if (st->hal->audio_position) {
        st->song_offset = st->hal->audio_position(st->hal);
      }
      if (st->hal->audio_stop) {
        st->hal->audio_stop(st->hal);
      }
      st->music = GCU_MUSIC_PAUSED;
    } else if (st->music == GCU_MUSIC_PAUSED || st->music == GCU_MUSIC_IDLE) {
      int off = (st->music == GCU_MUSIC_PAUSED) ? st->song_offset : 0;
      int ok = -1;
      int song_sz = st->hal->song_size ? st->hal->song_size(st->hal) : 0;
      const char *path = st->hal->song_path ? st->hal->song_path(st->hal) : NULL;
      if (path && song_sz > 0 && st->hal->play_file) {
        if (off < 0 || off >= song_sz) {
          off = 0;
        }
        ok = st->hal->play_file(st->hal, path, st->song_sample_rate_hz, off);
      } else if (st->song_pcm && st->song_pcm_len > 0 && st->hal->play_pcm) {
        if (off < 0 || off >= st->song_pcm_len) {
          off = 0;
        }
        ok = st->hal->play_pcm(st->hal, st->song_pcm + off,
                               st->song_pcm_len - off, st->song_sample_rate_hz);
      } else if (st->boot_pcm && st->boot_pcm_len > 0 && st->hal->play_pcm) {
        /* Fallback stand-in if SPIFFS First.pcm missing. */
        ok = st->hal->play_pcm(st->hal, st->boot_pcm, st->boot_pcm_len,
                               st->sample_rate_hz);
      }
      if (ok == 0) {
        st->music = GCU_MUSIC_PLAYING;
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
    /* Period matches dual-copy spacing so wrap is seamless (no full erase jump). */
    const int period = (int)strlen(GCU_BANNER_TEXT) * 6 * 2 + 48;
    st->banner_px = (st->banner_px + 1) % (period > 1 ? period : 1);
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

  if (st->screen == GCU_SCREEN_DETAILS) {
    if (now - st->last_details_ms >= 100) {
      st->last_details_ms = now;
      if (st->hal && st->hal->read_sensors) {
        (void)st->hal->read_sensors(st->hal, &st->sensors);
      }
      st->needs_details_values = 1;
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

/* Face layout constants — partial paints must match full paints. */
enum {
  GCU_EYE_Y = 70,
  GCU_EYE_LX = 110,
  GCU_EYE_RX = 210,
  GCU_SMILE_CY = 112,
  GCU_TITLE_Y = 162,
  GCU_HINT_Y0 = 200,
  GCU_HINT_H = 40,
};

/* Glyph hints per spec: color swatches / play-pause / gear. */
static void paint_button_glyphs(gcu_hal_t *hal, gcu_state_t *st,
                                const gcu_theme_colors_t *c) {
  if (!hal || !hal->fill_rect || !st || !c) {
    return;
  }
  /* Clear full hint band (avoids 1px residual bars between icons). */
  hal->fill_rect(hal, 0, GCU_HINT_Y0, GCU_LCD_W, GCU_HINT_H, c->bg565);

  /* Left (A): cycle-color glyph — four theme chips in a 2x2. */
  {
    int lx = 24, ly = 208;
    uint16_t sw[] = {rgb565(40, 140, 255), rgb565(255, 140, 0),
                     rgb565(255, 40, 40), rgb565(40, 220, 80)};
    for (int i = 0; i < 4; i++) {
      int col = i % 2;
      int row = i / 2;
      hal->fill_rect(hal, lx + col * 16, ly + row * 12, 14, 10, sw[i]);
    }
  }

  /* Middle (B): filled play triangle ▶ or pause ║. */
  {
    int mx = 148, my = 206;
    if (st->music == GCU_MUSIC_PLAYING) {
      hal->fill_rect(hal, mx + 2, my, 7, 22, c->ink565);
      hal->fill_rect(hal, mx + 14, my, 7, 22, c->ink565);
    } else {
      /* Right-pointing triangle via growing then shrinking rows. */
      for (int row = 0; row < 22; row++) {
        int from_mid = row < 11 ? row : (21 - row);
        int w = 4 + from_mid * 2;
        if (w > 20) {
          w = 20;
        }
        hal->fill_rect(hal, mx, my + row, w, 1, c->ink565);
      }
    }
  }

  /* Right (C): gear — ring with teeth + hub hole. */
  {
    int gx = 262, gy = 208;
    /* Outer teeth (cross + diagonals via blocks). */
    hal->fill_rect(hal, gx + 8, gy, 10, 4, c->ink565);
    hal->fill_rect(hal, gx + 8, gy + 22, 10, 4, c->ink565);
    hal->fill_rect(hal, gx, gy + 8, 4, 10, c->ink565);
    hal->fill_rect(hal, gx + 22, gy + 8, 4, 10, c->ink565);
    /* Body */
    hal->fill_rect(hal, gx + 4, gy + 4, 18, 18, c->ink565);
    /* Hub */
    hal->fill_rect(hal, gx + 10, gy + 10, 6, 6, c->bg565);
  }
}

/* Compose 5x7 glyph into a strip buffer (native RGB565). */
static void stamp_char(uint16_t *buf, int bw, int bh, int x, int y, char ch,
                       uint16_t fg, uint16_t bg, int scale) {
  unsigned char c = (unsigned char)ch;
  if (c >= 'a' && c <= 'z') {
    c = (unsigned char)(c - 'a' + 'A');
  }
  int idx = (c >= 32 && c <= 90) ? (int)(c - 32) : 0;
  for (int col = 0; col < 5; col++) {
    uint8_t bits = FONT5X7[idx][col];
    for (int row = 0; row < 7; row++) {
      uint16_t color = (bits & (1u << row)) ? fg : bg;
      for (int sy = 0; sy < scale; sy++) {
        for (int sx = 0; sx < scale; sx++) {
          int px = x + col * scale + sx;
          int py = y + row * scale + sy;
          if (px >= 0 && px < bw && py >= 0 && py < bh) {
            buf[py * bw + px] = color;
          }
        }
      }
    }
  }
}

static void paint_banner(gcu_state_t *st, const gcu_theme_colors_t *c) {
  gcu_hal_t *hal = st->hal;
  if (!hal || !c) {
    return;
  }
  /*
   * Scroll by shifting the strip left 1px and painting only the new right
   * column contents from a long virtual ribbon — no full-bar wipe.
   */
  enum { BH = 28, SCALE = 2 };
  static uint16_t strip[GCU_LCD_W * BH];
  static uint16_t last_hair, last_ink;
  static int primed;
  const int period = (int)strlen(GCU_BANNER_TEXT) * 6 * SCALE + 48;

  if (!primed || last_hair != c->hair565 || last_ink != c->ink565 ||
      st->needs_full_paint) {
    for (int i = 0; i < GCU_LCD_W * BH; i++) {
      strip[i] = c->hair565;
    }
    int x0 = -st->banner_px;
    while (x0 < GCU_LCD_W) {
      int cx = x0;
      for (const char *p = GCU_BANNER_TEXT; *p; p++) {
        stamp_char(strip, GCU_LCD_W, BH, cx, 6, *p, c->ink565, c->hair565, SCALE);
        cx += 6 * SCALE;
      }
      x0 += period;
    }
    primed = 1;
    last_hair = c->hair565;
    last_ink = c->ink565;
  } else {
    /* Shift left 1px (content moves left = scroll right-to-left). */
    for (int row = 0; row < BH; row++) {
      uint16_t *line = strip + row * GCU_LCD_W;
      memmove(line, line + 1, (size_t)(GCU_LCD_W - 1) * sizeof(uint16_t));
      line[GCU_LCD_W - 1] = c->hair565;
    }
    /* Stamp any glyph pixels that belong in the new rightmost column. */
    int virt = (st->banner_px + GCU_LCD_W - 1) % period;
    /* Find which character column this virtual x maps to. */
    int local = virt;
    if (local < (int)strlen(GCU_BANNER_TEXT) * 6 * SCALE) {
      int ci = local / (6 * SCALE);
      int gx = local % (6 * SCALE);
      if (ci >= 0 && ci < (int)strlen(GCU_BANNER_TEXT) && gx < 5 * SCALE) {
        char ch = GCU_BANNER_TEXT[ci];
        unsigned char uc = (unsigned char)ch;
        if (uc >= 'a' && uc <= 'z') {
          uc = (unsigned char)(uc - 'a' + 'A');
        }
        int idx = (uc >= 32 && uc <= 90) ? (int)(uc - 32) : 0;
        int font_col = gx / SCALE;
        int sub = gx % SCALE;
        (void)sub;
        if (font_col < 5) {
          uint8_t bits = FONT5X7[idx][font_col];
          for (int row = 0; row < 7; row++) {
            uint16_t color = (bits & (1u << row)) ? c->ink565 : c->hair565;
            for (int sy = 0; sy < SCALE; sy++) {
              int py = 6 + row * SCALE + sy;
              if (py >= 0 && py < BH) {
                strip[py * GCU_LCD_W + (GCU_LCD_W - 1)] = color;
              }
            }
          }
        }
      }
    }
  }

  if (hal->blit) {
    hal->blit(hal, 0, 0, GCU_LCD_W, BH, strip);
  } else if (hal->fill_rect) {
    hal->fill_rect(hal, 0, 0, GCU_LCD_W, BH, c->hair565);
  }
}

static void paint_eye(gcu_hal_t *hal, int cx, int cy, int closed, uint16_t fg,
                      uint16_t bg) {
  if (!hal || !hal->fill_rect) {
    return;
  }
  /* Soft classic emoji eye — clear only the eye disk, no under-lines. */
  int clear_r = 16;
  for (int dy = -clear_r; dy <= clear_r; dy++) {
    int half = 0;
    for (int t = clear_r; t >= 0; t--) {
      if (dy * dy + t * t <= clear_r * clear_r) {
        half = t;
        break;
      }
    }
    if (half > 0) {
      hal->fill_rect(hal, cx - half, cy + dy - 8, half * 2, 1, bg);
    }
  }
  /* Light eyebrow above */
  hal->fill_rect(hal, cx - 12, cy - 22, 24, 3, fg);

  if (closed) {
    /* Friendly closed curve */
    for (int i = 0; i < 3; i++) {
      hal->fill_rect(hal, cx - 10 + i, cy - 1 + i, 20 - 2 * i, 2, fg);
    }
  } else {
    /* Outer ring */
    int r = 12;
    for (int dy = -r; dy <= r; dy++) {
      int out_h = 0, in_h = 0;
      for (int t = r; t >= 0; t--) {
        if (dy * dy + t * t <= r * r) {
          out_h = t;
          break;
        }
      }
      int ri = 5;
      for (int t = ri; t >= 0; t--) {
        if (dy * dy + t * t <= ri * ri) {
          in_h = t;
          break;
        }
      }
      if (out_h > in_h) {
        hal->fill_rect(hal, cx - out_h, cy + dy, out_h - in_h, 1, fg);
        hal->fill_rect(hal, cx + in_h, cy + dy, out_h - in_h, 1, fg);
      } else if (out_h > 0 && in_h == 0) {
        /* top/bottom caps of ring — fill diameter for solid friendly dots */
      }
    }
    /* Filled friendly pupil/dot */
    int pr = 5;
    for (int dy = -pr; dy <= pr; dy++) {
      int half = 0;
      for (int t = pr; t >= 0; t--) {
        if (dy * dy + t * t <= pr * pr) {
          half = t;
          break;
        }
      }
      if (half > 0) {
        hal->fill_rect(hal, cx - half, cy + dy, half * 2, 1, fg);
      }
    }
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

  /* Eyes (face shifted up so play cue never covers smile). */
  paint_eye(hal, GCU_EYE_LX, GCU_EYE_Y, 0, c.fg565, c.bg565);
  paint_eye(hal, GCU_EYE_RX, GCU_EYE_Y, st->wink_closed, c.fg565, c.bg565);

  /* Smile arc higher on the panel. */
  {
    const int scx = 160;
    const int scy = GCU_SMILE_CY;
    const int ro = 40;
    const int ri = 30;
    for (int y = 8; y <= ro; y++) {
      int x_out = 0, x_in = 0;
      for (int t = ro; t >= 0; t--) {
        if (y * y + t * t <= ro * ro) {
          x_out = t;
          break;
        }
      }
      for (int t = ri; t >= 0; t--) {
        if (y * y + t * t <= ri * ri) {
          x_in = t;
          break;
        }
      }
      if (x_out > x_in) {
        int ypix = scy + y;
        hal->fill_rect(hal, scx - x_out, ypix, x_out - x_in, 1, c.fg565);
        hal->fill_rect(hal, scx + x_in, ypix, x_out - x_in, 1, c.fg565);
        if (y > ri - 2) {
          hal->fill_rect(hal, scx - x_out, ypix, x_out * 2, 2, c.fg565);
        }
      }
    }
  }

  /* Playing cue centered; gap above icons so descenders are not clipped. */
  if (st->music == GCU_MUSIC_PLAYING) {
    const char *title = "First by Tig";
    int tw = (int)strlen(title) * 12; /* scale-2: 6px * 2 */
    int tx = (GCU_LCD_W - tw) / 2;
    if (tx < 0) {
      tx = 0;
    }
    /* scale-2 text is 14px tall; leave clear air before GCU_HINT_Y0 */
    hal->fill_rect(hal, 0, GCU_TITLE_Y - 2, GCU_LCD_W, 20, c.bg565);
    draw_text(hal, tx, GCU_TITLE_Y, title, c.ink565, c.bg565, 2);
  }

  paint_button_glyphs(hal, st, &c);
}

static void paint_details_values(gcu_state_t *st, const gcu_theme_colors_t *c) {
  gcu_hal_t *hal = st->hal;
  if (!hal || !hal->fill_rect || !c) {
    return;
  }
  char line[40];
  /* Value strips only — labels stay put. Scale ~1.3× → use 2 for legibility. */
  const int x = 90;
  const int w = 220;
  const int sc = 2;
  const int row_h = 22;
  const gcu_sensors_t *s = &st->sensors;
  int y = 78;

  snprintf(line, sizeof line, "%+.1f %+.1f %+.1f", s->ax, s->ay, s->az);
  hal->fill_rect(hal, x, y, w, row_h, c->bg565);
  draw_text(hal, x, y, line, c->ink565, c->bg565, sc);
  y += row_h + 4;

  snprintf(line, sizeof line, "%+.0f %+.0f %+.0f", s->gx, s->gy, s->gz);
  hal->fill_rect(hal, x, y, w, row_h, c->bg565);
  draw_text(hal, x, y, line, c->ink565, c->bg565, sc);
  y += row_h + 4;

  snprintf(line, sizeof line, "%+.1f C", s->temp_c);
  hal->fill_rect(hal, x, y, w, row_h, c->bg565);
  draw_text(hal, x, y, line, c->ink565, c->bg565, sc);
  y += row_h + 4;

  snprintf(line, sizeof line, "%d %d %d", s->btn_a, s->btn_b, s->btn_c);
  hal->fill_rect(hal, x, y, w, row_h, c->bg565);
  draw_text(hal, x, y, line, c->ink565, c->bg565, sc);
  y += row_h + 4;

  snprintf(line, sizeof line, "%d", s->heap_free);
  hal->fill_rect(hal, x, y, w, row_h, c->bg565);
  draw_text(hal, x, y, line, c->ink565, c->bg565, sc);
  y += row_h + 4;

  const char *ms =
      st->music == GCU_MUSIC_PLAYING
          ? "PLAY"
          : (st->music == GCU_MUSIC_PAUSED ? "PAUSE" : "IDLE");
  snprintf(line, sizeof line, "%s", ms);
  hal->fill_rect(hal, x, y, w, row_h, c->bg565);
  draw_text(hal, x, y, line, c->ink565, c->bg565, sc);
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
  draw_text(hal, 8, 8, line, c.ink565, c.bg565, 2);
  draw_text(hal, 8, 40, "Details", c.fg565, c.bg565, 2);
  /* Labels scale 2 (~30%+ larger than prior scale-1). */
  int ly = 78;
  const int row_h = 26;
  draw_text(hal, 8, ly, "acc", c.ink565, c.bg565, 2);
  ly += row_h;
  draw_text(hal, 8, ly, "gyr", c.ink565, c.bg565, 2);
  ly += row_h;
  draw_text(hal, 8, ly, "tmp", c.ink565, c.bg565, 2);
  ly += row_h;
  draw_text(hal, 8, ly, "btn", c.ink565, c.bg565, 2);
  ly += row_h;
  draw_text(hal, 8, ly, "mem", c.ink565, c.bg565, 2);
  ly += row_h;
  draw_text(hal, 8, ly, "aud", c.ink565, c.bg565, 2);
  if (st->hal && st->hal->read_sensors) {
    (void)st->hal->read_sensors(st->hal, &st->sensors);
  }
  paint_details_values(st, &c);
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
    st->needs_details_values = 0;
    return;
  }

  if (st->screen == GCU_SCREEN_DETAILS) {
    if (st->needs_details_values) {
      gcu_theme_colors_t c = gcu_theme_colors(st->theme);
      paint_details_values(st, &c);
      st->needs_details_values = 0;
    }
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
    paint_eye(st->hal, GCU_EYE_RX, GCU_EYE_Y, st->wink_closed, c.fg565, c.bg565);
    st->needs_eye_paint = 0;
  }
  if (st->needs_hints_paint) {
    paint_button_glyphs(st->hal, st, &c);
    st->needs_hints_paint = 0;
  }
}
