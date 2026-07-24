#include "gcu/themes.h"

gcu_theme_t gcu_theme_next(gcu_theme_t t) {
  int n = (int)t + 1;
  if (n < 0 || n >= (int)GCU_THEME_COUNT) {
    n = 0;
  }
  return (gcu_theme_t)n;
}

const char *gcu_theme_name(gcu_theme_t t) {
  switch (t) {
  case GCU_THEME_BLUE:
    return "blue";
  case GCU_THEME_ORANGE:
    return "orange";
  case GCU_THEME_RED:
    return "red";
  case GCU_THEME_GREEN:
    return "green";
  case GCU_THEME_BLACK:
    return "black";
  default:
    return "blue";
  }
}

int gcu_theme_leds_off(gcu_theme_t t) { return t == GCU_THEME_BLACK ? 1 : 0; }

gcu_rgb_t gcu_theme_face(gcu_theme_t t) {
  switch (t) {
  case GCU_THEME_BLUE:
    return (gcu_rgb_t){0x33, 0x99, 0xff};
  case GCU_THEME_ORANGE:
    return (gcu_rgb_t){0xff, 0x99, 0x22};
  case GCU_THEME_RED:
    return (gcu_rgb_t){0xff, 0x33, 0x33};
  case GCU_THEME_GREEN:
    return (gcu_rgb_t){0x33, 0xcc, 0x55};
  case GCU_THEME_BLACK:
    return (gcu_rgb_t){0x00, 0x00, 0x00};
  default:
    return (gcu_rgb_t){0x33, 0x99, 0xff};
  }
}

gcu_rgb_t gcu_theme_bg(gcu_theme_t t) {
  switch (t) {
  case GCU_THEME_BLUE:
    return (gcu_rgb_t){0x00, 0x08, 0x1a};
  case GCU_THEME_ORANGE:
    return (gcu_rgb_t){0x1a, 0x0d, 0x00};
  case GCU_THEME_RED:
    return (gcu_rgb_t){0x1a, 0x00, 0x00};
  case GCU_THEME_GREEN:
    return (gcu_rgb_t){0x00, 0x14, 0x06};
  case GCU_THEME_BLACK:
    return (gcu_rgb_t){0xff, 0xff, 0xff}; /* black face on white bg */
  default:
    return (gcu_rgb_t){0x00, 0x08, 0x1a};
  }
}
