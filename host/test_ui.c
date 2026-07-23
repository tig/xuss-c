/* UI state machine against spec.md §4/§5 rules, on shipped defaults. */
#include "gcu/defaults.h"
#include "gcu/ui.h"

#include <stdio.h>

static int fails;

#define CHECK(cond)                                             \
  do {                                                          \
    if (!(cond)) {                                              \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      fails++;                                                  \
    }                                                           \
  } while (0)

static void test_theme_cycle(void) {
  gcu_ui_t ui;
  gcu_ui_init(&ui, 0);
  gcu_ui_take_dirty(&ui);
  int want[] = {GCU_THEME_ORANGE, GCU_THEME_RED, GCU_THEME_GREEN,
                GCU_THEME_BLACK, GCU_THEME_BLUE};
  for (int i = 0; i < 5; i++) {
    CHECK(gcu_ui_on_button(&ui, GCU_BTN_A, 0) == GCU_AUDIO_NONE);
    CHECK(ui.theme == want[i]);
    CHECK(gcu_ui_take_dirty(&ui) == GCU_DIRTY_THEME);
  }
}

static void test_song_state_machine(void) {
  gcu_ui_t ui;
  gcu_ui_init(&ui, 0);
  CHECK(gcu_ui_on_button(&ui, GCU_BTN_B, 0) == GCU_AUDIO_START);
  CHECK(ui.song == GCU_SONG_PLAYING);
  CHECK(gcu_ui_on_button(&ui, GCU_BTN_B, 0) == GCU_AUDIO_PAUSE);
  CHECK(ui.song == GCU_SONG_PAUSED);
  /* Resume, not restart. */
  CHECK(gcu_ui_on_button(&ui, GCU_BTN_B, 0) == GCU_AUDIO_RESUME);
  CHECK(ui.song == GCU_SONG_PLAYING);
  /* Natural end: idle, no loop. */
  gcu_ui_song_ended(&ui);
  CHECK(ui.song == GCU_SONG_IDLE);
  CHECK(gcu_ui_on_button(&ui, GCU_BTN_B, 0) == GCU_AUDIO_START);
}

static void test_a_while_playing_pauses_no_theme(void) {
  gcu_ui_t ui;
  gcu_ui_init(&ui, 0);
  gcu_ui_on_button(&ui, GCU_BTN_B, 0);
  int theme = ui.theme;
  CHECK(gcu_ui_on_button(&ui, GCU_BTN_A, 0) == GCU_AUDIO_PAUSE);
  CHECK(ui.song == GCU_SONG_PAUSED);
  CHECK(ui.theme == theme);
  CHECK(ui.screen == GCU_SCREEN_FACE);
}

static void test_details_rules(void) {
  gcu_ui_t ui;
  gcu_ui_init(&ui, 0);
  gcu_ui_on_button(&ui, GCU_BTN_B, 0); /* playing */
  /* C opens Details without pausing (§4.5). */
  CHECK(gcu_ui_on_button(&ui, GCU_BTN_C, 0) == GCU_AUDIO_NONE);
  CHECK(ui.screen == GCU_SCREEN_DETAILS);
  CHECK(ui.song == GCU_SONG_PLAYING);
  /* Second C is a no-op. */
  unsigned before = ui.dirty;
  CHECK(gcu_ui_on_button(&ui, GCU_BTN_C, 0) == GCU_AUDIO_NONE);
  CHECK(ui.screen == GCU_SCREEN_DETAILS);
  CHECK(ui.dirty == before);
  /* B on Details follows song rules; screen stays Details. */
  CHECK(gcu_ui_on_button(&ui, GCU_BTN_B, 0) == GCU_AUDIO_PAUSE);
  CHECK(ui.screen == GCU_SCREEN_DETAILS);
  /* A exits to face without theme change; music state unchanged. */
  int theme = ui.theme;
  CHECK(gcu_ui_on_button(&ui, GCU_BTN_A, 0) == GCU_AUDIO_NONE);
  CHECK(ui.screen == GCU_SCREEN_FACE);
  CHECK(ui.theme == theme);
  CHECK(ui.song == GCU_SONG_PAUSED);
}

static void test_mute_blocks_start(void) {
  gcu_ui_t ui;
  gcu_ui_init(&ui, 0);
  ui.mute = 1;
  CHECK(gcu_ui_on_button(&ui, GCU_BTN_B, 0) == GCU_AUDIO_NONE);
  CHECK(ui.song == GCU_SONG_IDLE);
  CHECK(ui.start_blocked == 1);
}

static void test_wink_time_based_eye_only(void) {
  gcu_ui_t ui;
  unsigned period = (unsigned)GCU_DEFAULTS.wink_period_ms;
  unsigned hold = (unsigned)GCU_DEFAULTS.wink_hold_ms;
  gcu_ui_init(&ui, 0);
  gcu_ui_take_dirty(&ui);

  /* Many ticks before the period: no wink, regardless of tick count. */
  for (unsigned t = 0; t < period - 1; t += period / 20) {
    gcu_ui_tick(&ui, t);
    CHECK(!(gcu_ui_take_dirty(&ui) & GCU_DIRTY_EYE));
    CHECK(!ui.eye_closed);
  }
  gcu_ui_tick(&ui, period);
  CHECK(ui.eye_closed);
  unsigned d = gcu_ui_take_dirty(&ui);
  CHECK(d & GCU_DIRTY_EYE);
  /* Wink repaints only the eye: no screen/theme flags. */
  CHECK(!(d & (GCU_DIRTY_SCREEN | GCU_DIRTY_THEME)));
  /* Eye reopens after the hold. */
  gcu_ui_tick(&ui, period + hold);
  CHECK(!ui.eye_closed);
  CHECK(gcu_ui_take_dirty(&ui) & GCU_DIRTY_EYE);
}

static void test_banner_continues_while_playing(void) {
  gcu_ui_t ui;
  unsigned step = (unsigned)GCU_DEFAULTS.banner_step_ms;
  gcu_ui_init(&ui, 0);
  gcu_ui_on_button(&ui, GCU_BTN_B, 0); /* playing */
  gcu_ui_take_dirty(&ui);
  int before = ui.banner_step;
  gcu_ui_tick(&ui, step);
  CHECK(ui.banner_step == before + 1);
  CHECK(gcu_ui_take_dirty(&ui) & GCU_DIRTY_BANNER);
}

static void test_details_refresh_only_when_visible(void) {
  gcu_ui_t ui;
  unsigned r = (unsigned)GCU_DEFAULTS.details_refresh_ms;
  gcu_ui_init(&ui, 0);
  gcu_ui_take_dirty(&ui);
  gcu_ui_tick(&ui, r);
  CHECK(!(gcu_ui_take_dirty(&ui) & GCU_DIRTY_DETAILS));
  gcu_ui_on_button(&ui, GCU_BTN_C, r);
  gcu_ui_take_dirty(&ui);
  gcu_ui_tick(&ui, 2 * r);
  CHECK(gcu_ui_take_dirty(&ui) & GCU_DIRTY_DETAILS);
}

int main(void) {
  test_theme_cycle();
  test_song_state_machine();
  test_a_while_playing_pauses_no_theme();
  test_details_rules();
  test_mute_blocks_start();
  test_wink_time_based_eye_only();
  test_banner_continues_while_playing();
  test_details_refresh_only_when_visible();
  if (fails) {
    return 1;
  }
  printf("OK ui\n");
  return 0;
}
