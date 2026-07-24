#include "gcu/render.h"

#include <stdio.h>
#include <string.h>

#define CHECK(c)                                                               \
  do {                                                                         \
    if (!(c)) {                                                                \
      fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c);              \
      return 1;                                                                \
    }                                                                          \
  } while (0)

/* Recording gfx double: tracks the union bbox of painted pixels and counts. */
typedef struct {
  gcu_gfx_t gfx;
  int rects;
  int texts;
  int minx, miny, maxx, maxy;
} rec_t;

static void note(rec_t *r, int x, int y, int w, int h) {
  if (x < r->minx)
    r->minx = x;
  if (y < r->miny)
    r->miny = y;
  if (x + w > r->maxx)
    r->maxx = x + w;
  if (y + h > r->maxy)
    r->maxy = y + h;
}

static void rec_fill(gcu_gfx_t *g, int x, int y, int w, int h, gcu_rgb_t c) {
  (void)c;
  rec_t *r = (rec_t *)g;
  r->rects++;
  note(r, x, y, w, h);
}

static void rec_text(gcu_gfx_t *g, int x, int y, const char *s, int scale,
                     gcu_rgb_t fg, gcu_rgb_t bg) {
  (void)fg;
  (void)bg;
  rec_t *r = (rec_t *)g;
  r->texts++;
  note(r, x, y, gcu_text_width(s, scale), 8 * scale);
}

static void rec_reset(rec_t *r) {
  r->rects = 0;
  r->texts = 0;
  r->minx = 100000;
  r->miny = 100000;
  r->maxx = -100000;
  r->maxy = -100000;
  r->gfx.width = 320;
  r->gfx.height = 240;
  r->gfx.fill_rect = rec_fill;
  r->gfx.text = rec_text;
}

int main(void) {
  /* text width: 8px per glyph per scale unit. */
  CHECK(gcu_text_width("AB", 1) == 16);
  CHECK(gcu_text_width("AB", 2) == 32);
  CHECK(gcu_text_width("", 2) == 0);
  CHECK(gcu_text_width(NULL, 2) == 0);

  rec_t r;
  gcu_face_view_t v = {GCU_THEME_BLUE, 0, 0, 0};

  /* Full face repaint spans the whole panel (mode-change clear). */
  rec_reset(&r);
  gcu_render_face(&r.gfx, &v);
  CHECK(r.minx <= 0 && r.miny <= 0);
  CHECK(r.maxx >= 320 && r.maxy >= 240);
  CHECK(r.rects > 3);

  /* Regional wink: right-eye repaint must NOT touch the hair bar, the left
   * eye, or the bottom hints — its bbox stays inside the right-eye column. */
  rec_reset(&r);
  gcu_render_eye(&r.gfx, &v, 1);
  CHECK(r.miny > 28);  /* below hair bar */
  CHECK(r.maxy < 210); /* above hints bar */
  CHECK(r.minx > 160); /* right half only (left eye untouched) */

  /* Playing cue only draws text when playing. */
  rec_reset(&r);
  v.playing = 0;
  gcu_render_cue(&r.gfx, &v);
  CHECK(r.texts == 0);
  rec_reset(&r);
  v.playing = 1;
  gcu_render_cue(&r.gfx, &v);
  CHECK(r.texts == 1);

  /* Hint glyph flips play/pause with playing state; left label is text,
   * middle/right are vector symbols (play/pause + gear). */
  rec_reset(&r);
  gcu_render_hints(&r.gfx, &v);
  CHECK(r.texts == 1); /* only the "color" label is text */
  CHECK(r.rects > 3);  /* pause/play + gear shapes */

  /* Details values are a regional repaint: 5 value fields, bbox on the right
   * side, never a full-panel clear. */
  rec_reset(&r);
  gcu_details_t d;
  memset(&d, 0, sizeof d);
  gcu_render_details_values(&r.gfx, GCU_THEME_BLUE, &d);
  CHECK(r.texts == 5);
  CHECK(r.minx >= 150); /* value column only, labels untouched */

  printf("OK render\n");
  return 0;
}
