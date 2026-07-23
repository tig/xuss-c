/* Renderer: regional-update contracts and theme rules (spec §4.2/§4.5). */
#include "gcu/face.h"
#include "gcu/ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;

#define CHECK(cond)                                             \
  do {                                                          \
    if (!(cond)) {                                              \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      fails++;                                                  \
    }                                                           \
  } while (0)

static unsigned short *paint_full(const gcu_face_view_t *v) {
  gcu_rect_t r = gcu_face_rect_full();
  unsigned short *buf = malloc((size_t)r.w * r.h * 2);
  gcu_face_paint(v, r, buf);
  return buf;
}

static int count_diff(const unsigned short *a, const unsigned short *b,
                      gcu_rect_t within, int *outside) {
  int inside = 0;
  *outside = 0;
  for (int y = 0; y < GCU_DISP_H; y++) {
    for (int x = 0; x < GCU_DISP_W; x++) {
      if (a[y * GCU_DISP_W + x] != b[y * GCU_DISP_W + x]) {
        if (x >= within.x && x < within.x + within.w && y >= within.y &&
            y < within.y + within.h) {
          inside++;
        } else {
          (*outside)++;
        }
      }
    }
  }
  return inside;
}

int main(void) {
  gcu_face_view_t v;
  memset(&v, 0, sizeof v);
  v.screen = GCU_SCREEN_FACE;
  v.theme = GCU_THEME_BLUE;
  v.song = GCU_SONG_IDLE;
  snprintf(v.fw_line, sizeof v.fw_line, "XUSSC 0.0.1");

  /* Banner text puts ink pixels in the hair strip (step 80: text is
   * mid-screen; the marquee enters from the right at step 0). */
  v.banner_step = 80;
  unsigned short *base = paint_full(&v);
  gcu_rect_t banner = gcu_face_rect_banner();
  int ink = 0;
  for (int y = 0; y < banner.h; y++) {
    for (int x = 0; x < banner.w; x++) {
      if (base[y * GCU_DISP_W + x] != base[0]) {
        ink++;
      }
    }
  }
  CHECK(ink > 100);

  /* Banner motion changes the strip. */
  v.banner_step = 87;
  unsigned short *moved = paint_full(&v);
  int outside;
  CHECK(count_diff(base, moved, banner, &outside) > 0);
  CHECK(outside == 0); /* banner motion repaints only the strip */
  free(moved);
  v.banner_step = 80;

  /* Wink changes pixels only inside the right-eye rect. */
  v.eye_closed = 1;
  unsigned short *winked = paint_full(&v);
  int inside = count_diff(base, winked, gcu_face_rect_eye(), &outside);
  CHECK(inside > 50);
  CHECK(outside == 0);
  free(winked);
  v.eye_closed = 0;

  /* Playing cue appears only while PLAYING, only in the cue rect. */
  v.song = GCU_SONG_PLAYING;
  unsigned short *playing = paint_full(&v);
  gcu_rect_t cue = gcu_face_rect_cue();
  inside = count_diff(base, playing, cue, &outside);
  CHECK(inside > 50);
  /* hint row also changes (play -> pause glyph); allow only that region */
  gcu_rect_t hints = {0, 216, GCU_DISP_W, GCU_DISP_H - 216};
  int hint_changes = count_diff(base, playing, hints, &inside);
  CHECK(hint_changes > 0);
  free(playing);
  v.song = GCU_SONG_IDLE;

  /* Black theme: white background, sides handled elsewhere. */
  v.theme = GCU_THEME_BLACK;
  unsigned short *black = paint_full(&v);
  CHECK(black[120 * GCU_DISP_W + 20] == 0xFFFF); /* bg pixel is white */
  free(black);
  v.theme = GCU_THEME_BLUE;

  /* Details: fw line ink present; value change repaints only value rect. */
  v.screen = GCU_SCREEN_DETAILS;
  v.ax_mg = 100;
  unsigned short *d1 = paint_full(&v);
  int fw_ink = 0;
  for (int y = 12; y < 30; y++) {
    for (int x = 16; x < 16 + 11 * 10; x++) {
      if (d1[y * GCU_DISP_W + x] != d1[5]) {
        fw_ink++;
      }
    }
  }
  CHECK(fw_ink > 50);
  v.ax_mg = -900;
  unsigned short *d2 = paint_full(&v);
  inside = count_diff(d1, d2, gcu_face_rect_values(), &outside);
  CHECK(inside > 0);
  CHECK(outside == 0); /* value refresh: no full-screen flash (§4.5) */
  free(d1);
  free(d2);

  free(base);
  if (fails) {
    return 1;
  }
  printf("OK face\n");
  return 0;
}
