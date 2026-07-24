#include "gcu/app.h"

#include <stdio.h>

#define CHECK(c)                                                               \
  do {                                                                         \
    if (!(c)) {                                                                \
      fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c);              \
      return 1;                                                                \
    }                                                                          \
  } while (0)

int main(void) {
  gcu_app_t app;
  gcu_app_init(&app);
  CHECK(app.theme == GCU_THEME_BLUE);
  CHECK(app.screen == GCU_SCREEN_FACE);
  CHECK(!gcu_app_music_playing(&app));

  /* A on face (not playing) advances the theme. */
  gcu_app_button(&app, GCU_BTN_A);
  CHECK(app.theme == GCU_THEME_ORANGE);

  /* B starts music; screen stays on the living face. */
  gcu_app_button(&app, GCU_BTN_B);
  CHECK(gcu_app_music_playing(&app));
  CHECK(app.screen == GCU_SCREEN_FACE);

  /* A while playing pauses and does NOT advance the theme. */
  gcu_app_button(&app, GCU_BTN_A);
  CHECK(!gcu_app_music_playing(&app));
  CHECK(app.theme == GCU_THEME_ORANGE); /* unchanged */
  CHECK(app.screen == GCU_SCREEN_FACE);

  /* B resumes from pause. */
  gcu_app_button(&app, GCU_BTN_B);
  CHECK(gcu_app_music_playing(&app));

  /* C opens Details WITHOUT pausing the music. */
  gcu_app_button(&app, GCU_BTN_C);
  CHECK(app.screen == GCU_SCREEN_DETAILS);
  CHECK(gcu_app_music_playing(&app));

  /* C on Details is a no-op. */
  gcu_app_button(&app, GCU_BTN_C);
  CHECK(app.screen == GCU_SCREEN_DETAILS);

  /* A on Details exits to face, theme unchanged, music unchanged. */
  gcu_app_button(&app, GCU_BTN_A);
  CHECK(app.screen == GCU_SCREEN_FACE);
  CHECK(app.theme == GCU_THEME_ORANGE);
  CHECK(gcu_app_music_playing(&app));

  /* mute blocks a fresh start: pause to idle, set mute, B stays idle. */
  gcu_app_button(&app, GCU_BTN_B); /* pause */
  app.music.phase = GCU_MUSIC_IDLE;
  app.music.offset = 0;
  app.cfg.mute = 1;
  gcu_app_button(&app, GCU_BTN_B);
  CHECK(!gcu_app_music_playing(&app));

  printf("OK app\n");
  return 0;
}
