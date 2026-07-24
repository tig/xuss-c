#ifndef GCU_THEMES_H
#define GCU_THEMES_H

/* Face/side-LED themes (§4.3). Cycle: blue -> orange -> red -> green -> black. */

typedef enum {
  GCU_THEME_BLUE = 0,
  GCU_THEME_ORANGE,
  GCU_THEME_RED,
  GCU_THEME_GREEN,
  GCU_THEME_BLACK,
  GCU_THEME_COUNT
} gcu_theme_t;

typedef struct {
  unsigned char r, g, b;
} gcu_rgb_t;

/* One edge per press; wraps black -> blue. */
gcu_theme_t gcu_theme_next(gcu_theme_t t);
const char *gcu_theme_name(gcu_theme_t t);

/* Black theme forces side LEDs fully off (§4.3, §4.2). */
int gcu_theme_leds_off(gcu_theme_t t);

gcu_rgb_t gcu_theme_face(gcu_theme_t t);
gcu_rgb_t gcu_theme_bg(gcu_theme_t t);

#endif
