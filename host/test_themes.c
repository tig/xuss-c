#include "gcu/themes.h"

#include <stdio.h>
#include <string.h>

#define CHECK(c)                                                               \
  do {                                                                         \
    if (!(c)) {                                                                \
      fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c);              \
      return 1;                                                                \
    }                                                                          \
  } while (0)

int main(void) {
  /* Fixed cycle order blue -> orange -> red -> green -> black -> blue. */
  gcu_theme_t t = GCU_THEME_BLUE;
  CHECK((t = gcu_theme_next(t)) == GCU_THEME_ORANGE);
  CHECK((t = gcu_theme_next(t)) == GCU_THEME_RED);
  CHECK((t = gcu_theme_next(t)) == GCU_THEME_GREEN);
  CHECK((t = gcu_theme_next(t)) == GCU_THEME_BLACK);
  CHECK((t = gcu_theme_next(t)) == GCU_THEME_BLUE); /* wrap */

  CHECK(strcmp(gcu_theme_name(GCU_THEME_BLUE), "blue") == 0);
  CHECK(strcmp(gcu_theme_name(GCU_THEME_BLACK), "black") == 0);

  /* Only black kills the side LEDs; black uses a white background. */
  CHECK(gcu_theme_leds_off(GCU_THEME_BLUE) == 0);
  CHECK(gcu_theme_leds_off(GCU_THEME_BLACK) == 1);
  gcu_rgb_t bg = gcu_theme_bg(GCU_THEME_BLACK);
  CHECK(bg.r == 0xff && bg.g == 0xff && bg.b == 0xff);
  gcu_rgb_t face = gcu_theme_face(GCU_THEME_BLACK);
  CHECK(face.r == 0 && face.g == 0 && face.b == 0);

  printf("OK themes\n");
  return 0;
}
