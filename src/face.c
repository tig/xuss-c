#include "gcu/face.h"

#include "gcu/font.h"
#include "gcu/ui.h"

#include <stdio.h>
#include <string.h>

#define RGB(r, g, b) \
  (unsigned short)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

/* Layout (320x240 landscape). */
#define BANNER_H 24
#define EYE_Y 92
#define EYE_R 22
#define EYE_LX 110
#define EYE_RX 210
#define SMILE_CY 148
#define SMILE_R 56
#define CUE_Y 186
#define CUE_H 22
#define HINT_Y 216
#define VAL_X 144
#define VAL_W 176 /* fits "-2000 -2000 -2000" (17 cells) — review P2 */
#define ROW0_Y 44
#define ROW_H 26

static const char BANNER_TEXT[] = "Xuss-C; built on ESP-IDF";
#define BANNER_SPEED 2 /* px per banner step */

typedef struct {
  unsigned short face, bg, banner_bg, ink;
} theme_pal_t;

static const theme_pal_t PAL[GCU_THEME_COUNT] = {
    {RGB(60, 130, 255), RGB(0, 0, 40), RGB(0, 10, 80), RGB(170, 205, 255)},
    {RGB(255, 145, 25), RGB(45, 18, 0), RGB(85, 40, 0), RGB(255, 205, 150)},
    {RGB(255, 55, 55), RGB(45, 0, 0), RGB(85, 8, 8), RGB(255, 170, 170)},
    {RGB(50, 230, 90), RGB(0, 40, 10), RGB(0, 75, 22), RGB(175, 255, 195)},
    {RGB(0, 0, 0), RGB(255, 255, 255), RGB(225, 225, 225), RGB(0, 0, 0)},
};

gcu_rect_t gcu_face_rect_full(void) {
  gcu_rect_t r = {0, 0, GCU_DISP_W, GCU_DISP_H};
  return r;
}
gcu_rect_t gcu_face_rect_banner(void) {
  gcu_rect_t r = {0, 0, GCU_DISP_W, BANNER_H};
  return r;
}
gcu_rect_t gcu_face_rect_eye(void) {
  gcu_rect_t r = {EYE_RX - EYE_R - 2, EYE_Y - EYE_R - 2, 2 * EYE_R + 4,
                  2 * EYE_R + 4};
  return r;
}
gcu_rect_t gcu_face_rect_cue(void) {
  gcu_rect_t r = {0, CUE_Y, GCU_DISP_W, CUE_H};
  return r;
}
gcu_rect_t gcu_face_rect_values(void) {
  gcu_rect_t r = {VAL_X, ROW0_Y, VAL_W, 6 * ROW_H};
  return r;
}

/* 1 when pixel (px,py) is inside text drawn with its top-left at (tx,ty). */
static int text_hit(const char *s, int tx, int ty, int px, int py) {
  if (py < ty || py >= ty + GCU_FONT_H || px < tx) {
    return 0;
  }
  int idx = (px - tx) / GCU_FONT_W;
  int len = (int)strlen(s);
  if (idx >= len) {
    return 0;
  }
  char c = s[idx];
  if (c < GCU_FONT_FIRST || c > GCU_FONT_LAST) {
    return 0;
  }
  int gx = (px - tx) % GCU_FONT_W;
  int gy = py - ty;
  const unsigned char *g = GCU_FONT[c - GCU_FONT_FIRST];
  unsigned bits = ((unsigned)g[gy * GCU_FONT_ROW_BYTES] << 8) |
                  g[gy * GCU_FONT_ROW_BYTES + 1];
  return (bits & (0x8000u >> gx)) != 0;
}

static int in_circle(int cx, int cy, int r, int x, int y) {
  int dx = x - cx, dy = y - cy;
  return dx * dx + dy * dy <= r * r;
}

/* --- face screen ------------------------------------------------------ */

static unsigned short face_pixel(const gcu_face_view_t *v,
                                 const theme_pal_t *p, int x, int y) {
  /* Hair banner strip. */
  if (y < BANNER_H) {
    int text_w = (int)(sizeof BANNER_TEXT - 1) * GCU_FONT_W;
    int span = GCU_DISP_W + text_w;
    /* unsigned math: step*speed may wrap; C signed % would go negative
     * and blank the marquee (review P2) */
    int off = (int)(((unsigned)v->banner_step * BANNER_SPEED) % (unsigned)span);
    int tx = GCU_DISP_W - off;
    if (text_hit(BANNER_TEXT, tx, (BANNER_H - GCU_FONT_H) / 2, x, y)) {
      return p->ink;
    }
    return p->banner_bg;
  }

  /* Eyes: right eye winks (closed = thin bar), left stays open. */
  if (in_circle(EYE_LX, EYE_Y, EYE_R, x, y)) {
    return p->face;
  }
  if (v->eye_closed) {
    if (x >= EYE_RX - EYE_R && x <= EYE_RX + EYE_R && y >= EYE_Y - 3 &&
        y <= EYE_Y + 3) {
      return p->face;
    }
  } else if (in_circle(EYE_RX, EYE_Y, EYE_R, x, y)) {
    return p->face;
  }

  /* Smile: lower arc band, clipped above the cue line so the playing
   * title is never overdrawn (review P1). */
  if (y < CUE_Y) {
    int dx = x - (GCU_DISP_W / 2), dy = y - SMILE_CY;
    int d2 = dx * dx + dy * dy;
    int lo = (SMILE_R - 4) * (SMILE_R - 4), hi = (SMILE_R + 4) * (SMILE_R + 4);
    if (d2 >= lo && d2 <= hi && dy > SMILE_R / 3) {
      return p->face;
    }
  }

  /* Playing cue (title visible while the song plays — §4.2). */
  if (v->song == GCU_SONG_PLAYING && y >= CUE_Y && y < CUE_Y + CUE_H) {
    if (text_hit("First by Tig", (GCU_DISP_W - 12 * GCU_FONT_W) / 2,
                 CUE_Y + 2, x, y)) {
      return p->ink;
    }
  }

  /* Button hints. */
  if (y >= HINT_Y) {
    if (text_hit("color", 36, HINT_Y + 3, x, y)) {
      return p->ink;
    }
    if (v->song == GCU_SONG_PLAYING) {
      /* pause: two bars */
      if (y >= HINT_Y + 4 && y < HINT_Y + 20 &&
          ((x >= 154 && x < 159) || (x >= 163 && x < 168))) {
        return p->ink;
      }
    } else {
      /* play: right-pointing triangle */
      int dx = x - 154, dy = y - (HINT_Y + 12);
      if (dx >= 0 && dx < 14 && dy >= -(14 - dx) / 2 && dy <= (14 - dx) / 2) {
        return p->ink;
      }
    }
    /* gear: ring + 4 teeth */
    if ((in_circle(276, HINT_Y + 12, 9, x, y) &&
         !in_circle(276, HINT_Y + 12, 4, x, y)) ||
        (x >= 274 && x <= 278 && (y == HINT_Y + 1 || y == HINT_Y + 23)) ||
        (y >= HINT_Y + 10 && y <= HINT_Y + 14 && (x == 265 || x == 287))) {
      return p->ink;
    }
  }

  return p->bg;
}

/* --- Details screen --------------------------------------------------- */

static unsigned short details_pixel(const gcu_face_view_t *v,
                                    const theme_pal_t *p, int x, int y) {
  char buf[32];
  /* Firmware identity header (readable digits — §4.5). */
  if (text_hit(v->fw_line, 16, 12, x, y)) {
    return p->ink;
  }
  static const char *labels[] = {"accel mg", "gyro dps", "imu temp",
                                 "heap", "held", "song"};
  for (int i = 0; i < 6; i++) {
    if (text_hit(labels[i], 16, ROW0_Y + i * ROW_H + 4, x, y)) {
      return p->ink;
    }
  }
  if (x >= VAL_X) {
    int row = (y - ROW0_Y) / ROW_H;
    switch (row) {
      case 0:
        snprintf(buf, sizeof buf, "%d %d %d", v->ax_mg, v->ay_mg, v->az_mg);
        break;
      case 1:
        snprintf(buf, sizeof buf, "%d %d %d", v->gx_dps, v->gy_dps, v->gz_dps);
        break;
      case 2: {
        int t = v->temp_dc < 0 ? -v->temp_dc : v->temp_dc;
        snprintf(buf, sizeof buf, "%s%d.%d C", v->temp_dc < 0 ? "-" : "",
                 t / 10, t % 10); /* keeps the sign for -0.x (review P2) */
        break;
      }
      case 3:
        snprintf(buf, sizeof buf, "%u", v->heap_free);
        break;
      case 4:
        snprintf(buf, sizeof buf, "%s%s%s", v->btn_a ? "A" : "-",
                 v->btn_b ? "B" : "-", v->btn_c ? "C" : "-");
        break;
      case 5:
        snprintf(buf, sizeof buf, "%s", v->song == GCU_SONG_PLAYING  ? "playing"
                                        : v->song == GCU_SONG_PAUSED ? "paused"
                                                                     : "idle");
        break;
      default:
        return p->bg;
    }
    if (row >= 0 && row < 6 &&
        text_hit(buf, VAL_X, ROW0_Y + row * ROW_H + 4, x, y)) {
      return p->ink;
    }
  }
  return p->bg;
}

void gcu_face_paint(const gcu_face_view_t *v, gcu_rect_t rect,
                    unsigned short *buf) {
  const theme_pal_t *p = &PAL[v->theme >= 0 && v->theme < GCU_THEME_COUNT
                                  ? v->theme
                                  : GCU_THEME_BLUE];
  for (int ry = 0; ry < rect.h; ry++) {
    int y = rect.y + ry;
    for (int rx = 0; rx < rect.w; rx++) {
      int x = rect.x + rx;
      buf[ry * rect.w + rx] = v->screen == GCU_SCREEN_DETAILS
                                  ? details_pixel(v, p, x, y)
                                  : face_pixel(v, p, x, y);
    }
  }
}
