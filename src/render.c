#include "gcu/render.h"

#include "gcu/defaults.h"

#include <stdio.h>
#include <string.h>

/* -------- panel layout (320x240 IPS) -------- */
#define W 320
#define H 240
#define HAIR_H 28
#define HINT_H 30
#define HINT_Y (H - HINT_H)

#define EYE_R 26
#define EYE_CY 92
#define EYE_L_CX 108
#define EYE_R_CX 212

#define SMILE_CX 160
#define SMILE_HALF 60
#define SMILE_TOP 150
#define SMILE_DEPTH 26
#define SMILE_T 8

#define CUE_Y 184
#define CUE_SCALE 2
#define BANNER_SCALE 2
#define BANNER_GAP 40

#define DET_TOP 22
#define DET_ROW_H 16
#define DET_LABEL_X 8
#define DET_VALUE_X 96
#define DET_SCALE 1

int gcu_text_width(const char *s, int scale) {
  if (!s || scale < 1) {
    return 0;
  }
  return (int)strlen(s) * GCU_GLYPH_W * scale;
}

static int isqrt_i(int v) {
  if (v <= 0) {
    return 0;
  }
  int x = v, y = (x + 1) / 2;
  while (y < x) {
    x = y;
    y = (x + v / x) / 2;
  }
  return x;
}

static void fill_circle(gcu_gfx_t *g, int cx, int cy, int r, gcu_rgb_t c) {
  for (int dy = -r; dy <= r; dy++) {
    int dx = isqrt_i(r * r - dy * dy);
    g->fill_rect(g, cx - dx, cy + dy, 2 * dx + 1, 1, c);
  }
}

/* Center a string horizontally within [x0, x0+w). */
static int center_x(const char *s, int scale, int x0, int w) {
  int tw = gcu_text_width(s, scale);
  int x = x0 + (w - tw) / 2;
  return x < x0 ? x0 : x;
}

int gcu_render_banner_span(int scale) {
  return gcu_text_width(GCU_DEFAULTS.banner_text, scale) + BANNER_GAP * scale;
}

void gcu_render_hair(gcu_gfx_t *g, const gcu_face_view_t *v) {
  gcu_rgb_t face = gcu_theme_face(v->theme);
  gcu_rgb_t bg = gcu_theme_bg(v->theme);
  /* The hair bar is the theme-colored band; banner ink is the bg color. */
  g->fill_rect(g, 0, 0, W, HAIR_H, face);
  int span = gcu_render_banner_span(BANNER_SCALE);
  int off = v->banner_offset % span;
  int ty = (HAIR_H - GCU_GLYPH_H * BANNER_SCALE) / 2;
  for (int x = -off; x < W; x += span) {
    g->text(g, x, ty, GCU_DEFAULTS.banner_text, BANNER_SCALE, bg, face);
  }
}

void gcu_render_eye(gcu_gfx_t *g, const gcu_face_view_t *v, int right) {
  gcu_rgb_t face = gcu_theme_face(v->theme);
  gcu_rgb_t bg = gcu_theme_bg(v->theme);
  int cx = right ? EYE_R_CX : EYE_L_CX;
  /* Regional repaint: clear only this eye's box, then draw open or closed. */
  g->fill_rect(g, cx - EYE_R, EYE_CY - EYE_R, 2 * EYE_R + 1, 2 * EYE_R + 1, bg);
  if (right && v->wink_closed) {
    g->fill_rect(g, cx - EYE_R, EYE_CY - 3, 2 * EYE_R + 1, 6, face);
  } else {
    fill_circle(g, cx, EYE_CY, EYE_R, face);
  }
}

static void render_smile(gcu_gfx_t *g, const gcu_face_view_t *v) {
  gcu_rgb_t face = gcu_theme_face(v->theme);
  for (int x = -SMILE_HALF; x <= SMILE_HALF; x += 2) {
    /* U-shaped smile: lowest at center, rising toward the corners. */
    int y = SMILE_TOP + (SMILE_DEPTH * (SMILE_HALF * SMILE_HALF - x * x)) /
                            (SMILE_HALF * SMILE_HALF);
    g->fill_rect(g, SMILE_CX + x, y, 2, SMILE_T, face);
  }
}

void gcu_render_cue(gcu_gfx_t *g, const gcu_face_view_t *v) {
  gcu_rgb_t face = gcu_theme_face(v->theme);
  gcu_rgb_t bg = gcu_theme_bg(v->theme);
  int h = GCU_GLYPH_H * CUE_SCALE;
  /* Regional: clear the cue strip; draw the title only while playing. */
  g->fill_rect(g, 0, CUE_Y, W, h, bg);
  if (v->playing) {
    const char *cue = "First by Tig";
    g->text(g, center_x(cue, CUE_SCALE, 0, W), CUE_Y, cue, CUE_SCALE, face, bg);
  }
}

/* --- button-hint glyphs drawn as vector shapes (font is ASCII-only, §4.2) --- */

static void hint_play(gcu_gfx_t *g, int cx, int cy, gcu_rgb_t c) {
  int h = 20, w = 16; /* right-pointing triangle, flat left edge */
  for (int y = 0; y < h; y++) {
    int d = y <= h / 2 ? y : h - 1 - y;
    int xr = (w * d) / (h / 2);
    g->fill_rect(g, cx - w / 2, cy - h / 2 + y, xr + 1, 1, c);
  }
}

static void hint_pause(gcu_gfx_t *g, int cx, int cy, gcu_rgb_t c) {
  int h = 20, bw = 5, gap = 6;
  g->fill_rect(g, cx - gap / 2 - bw, cy - h / 2, bw, h, c);
  g->fill_rect(g, cx + gap / 2, cy - h / 2, bw, h, c);
}

static void hint_gear(gcu_gfx_t *g, int cx, int cy, gcu_rgb_t c, gcu_rgb_t bg) {
  static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
  int R = 10;
  fill_circle(g, cx, cy, R, c);
  for (int i = 0; i < 8; i++) {
    g->fill_rect(g, cx + dx[i] * R - 2, cy + dy[i] * R - 2, 4, 4, c);
  }
  fill_circle(g, cx, cy, 4, bg); /* hub hole */
}

void gcu_render_hints(gcu_gfx_t *g, const gcu_face_view_t *v) {
  gcu_rgb_t face = gcu_theme_face(v->theme);
  gcu_rgb_t bg = gcu_theme_bg(v->theme);
  g->fill_rect(g, 0, HINT_Y, W, HINT_H, bg);
  int cy = HINT_Y + HINT_H / 2;

  /* Left: the word "color" (§4.2 hint copy). */
  const char *lbl = "color";
  int lx = center_x(lbl, 2, 0, W / 3);
  g->text(g, lx, cy - GCU_GLYPH_H, lbl, 2, face, bg);

  /* Middle: play/pause glyph (symbol, not text). */
  if (v->playing) {
    hint_pause(g, W / 2, cy, face);
  } else {
    hint_play(g, W / 2, cy, face);
  }

  /* Right: gear glyph (opens Details). */
  hint_gear(g, 5 * W / 6, cy, face, bg);
}

void gcu_render_face(gcu_gfx_t *g, const gcu_face_view_t *v) {
  gcu_rgb_t bg = gcu_theme_bg(v->theme);
  g->fill_rect(g, 0, 0, W, H, bg); /* mode-change full clear */
  gcu_render_hair(g, v);
  gcu_render_eye(g, v, 0);
  gcu_render_eye(g, v, 1);
  render_smile(g, v);
  gcu_render_cue(g, v);
  gcu_render_hints(g, v);
}

/* -------- Details (§4.5) -------- */

static const char *const DET_LABELS[6] = {"AX AY AZ", "GX GY GZ", "TEMP C",
                                          "BTN ABC", "HEAP KB", ""};

void gcu_render_details_chrome(gcu_gfx_t *g, gcu_theme_t theme,
                               const char *id_line) {
  gcu_rgb_t face = gcu_theme_face(theme);
  gcu_rgb_t bg = gcu_theme_bg(theme);
  g->fill_rect(g, 0, 0, W, H, bg); /* mode-change full clear */
  g->text(g, DET_LABEL_X, 4, "DETAILS", 1, face, bg);
  if (id_line) {
    g->text(g, W - gcu_text_width(id_line, 1) - DET_LABEL_X, 4, id_line, 1, face,
            bg);
  }
  for (int i = 0; i < 5; i++) {
    int y = DET_TOP + (i + 1) * DET_ROW_H;
    g->text(g, DET_LABEL_X, y, DET_LABELS[i], DET_SCALE, face, bg);
  }
}

void gcu_render_details_values(gcu_gfx_t *g, gcu_theme_t theme,
                               const gcu_details_t *d) {
  gcu_rgb_t face = gcu_theme_face(theme);
  gcu_rgb_t bg = gcu_theme_bg(theme);
  char buf[48];
  char a[3][12], b[3][12];
  for (int i = 0; i < 3; i++) {
    gcu_fmt_milli1(a[i], sizeof a[i], d->accel_mg[i]);
    gcu_fmt_milli1(b[i], sizeof b[i], d->gyro_mdps[i]);
  }
  int valw = W - DET_VALUE_X;

  for (int i = 0; i < 5; i++) {
    int y = DET_TOP + (i + 1) * DET_ROW_H;
    switch (i) {
    case 0:
      snprintf(buf, sizeof buf, "%s %s %s", a[0], a[1], a[2]);
      break;
    case 1:
      snprintf(buf, sizeof buf, "%s %s %s", b[0], b[1], b[2]);
      break;
    case 2: {
      char t[12];
      gcu_fmt_milli1(t, sizeof t, d->imu_temp_mc);
      snprintf(buf, sizeof buf, "%s", t);
      break;
    }
    case 3:
      snprintf(buf, sizeof buf, "%d %d %d", d->button[0], d->button[1],
               d->button[2]);
      break;
    default:
      snprintf(buf, sizeof buf, "%ld", d->heap_free / 1024);
      break;
    }
    /* Regional: clear and repaint just this value field. */
    g->fill_rect(g, DET_VALUE_X, y, valw, GCU_GLYPH_H * DET_SCALE, bg);
    g->text(g, DET_VALUE_X, y, buf, DET_SCALE, face, bg);
  }
}
