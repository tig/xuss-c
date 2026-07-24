#ifndef GCU_GFX_H
#define GCU_GFX_H

#include "gcu/themes.h"

/* Portable 2D primitive contract (§4.2, §5.2). The metal backend
 * (hal_display) implements these against the IPS panel; host tests pass a
 * recording double. Geometry/layout lives in render.c so "what is drawn where"
 * is testable without hardware; only the pixels are L1.
 *
 * Text is a fixed 8x8 cell font. `scale` >= 1 multiplies the cell. Off-panel
 * or partially clipped draws are the backend's responsibility to clamp. */

#define GCU_GLYPH_W 8
#define GCU_GLYPH_H 8

typedef struct gcu_gfx gcu_gfx_t;

struct gcu_gfx {
  int width;
  int height;
  void (*fill_rect)(gcu_gfx_t *self, int x, int y, int w, int h, gcu_rgb_t c);
  void (*text)(gcu_gfx_t *self, int x, int y, const char *s, int scale,
               gcu_rgb_t fg, gcu_rgb_t bg);
};

/* Pixel width of a string at the given scale (space between glyphs = 0). */
int gcu_text_width(const char *s, int scale);

#endif
