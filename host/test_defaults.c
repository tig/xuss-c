#include "gcu/defaults.h"
#include "gcu/domain.h"
#include "gcu/version.h"

#include <stdio.h>
#include <string.h>

static int64_t g_now;

static int64_t fake_now(gcu_hal_t *self) {
  (void)self;
  return g_now;
}

static gcu_hal_t g_hal = {.set_led = NULL, .delay_ms = NULL, .now_ms = fake_now};

int main(void) {
  char id[64];
  gcu_state_t st;
  gcu_view_t view;

  if (GCU_DEFAULTS.pin_side_led != 15 || GCU_DEFAULTS.pin_speaker != 25) {
    fprintf(stderr, "m5go pin pack missing from defaults\n");
    return 1;
  }
  if (GCU_DEFAULTS.wink_period_ms != 10000) {
    fprintf(stderr, "wink period not shipped default\n");
    return 1;
  }
  if (gcu_theme_count() != 5) {
    fprintf(stderr, "theme count %d\n", gcu_theme_count());
    return 1;
  }

  gcu_identity_line(id, (int)sizeof id);
  if (strstr(id, "fw_name=XUSSC") == NULL ||
      strstr(id, "fw_version=0.0.1") == NULL) {
    fprintf(stderr, "identity line bad: %s\n", id);
    return 1;
  }

  g_now = 0;
  gcu_init(&st, &g_hal);
  gcu_tick(&st, &view);
  if (!view.request_boot_riff) {
    fprintf(stderr, "boot riff not requested at init\n");
    return 1;
  }
  if (view.theme_index != 0 || !view.sides_on) {
    fprintf(stderr, "default theme not blue with sides on\n");
    return 1;
  }

  /* Theme cycle via button A */
  for (int i = 1; i < 5; i++) {
    gcu_on_button(&st, GCU_BTN_A);
    gcu_tick(&st, &view);
    if (view.theme_index != i) {
      fprintf(stderr, "theme index want %d got %d\n", i, view.theme_index);
      return 1;
    }
  }
  gcu_on_button(&st, GCU_BTN_A);
  gcu_tick(&st, &view);
  if (view.theme_index != 0) {
    fprintf(stderr, "theme wrap failed\n");
    return 1;
  }
  /* black is index 4 — sides off */
  gcu_on_button(&st, GCU_BTN_A);
  gcu_on_button(&st, GCU_BTN_A);
  gcu_on_button(&st, GCU_BTN_A);
  gcu_on_button(&st, GCU_BTN_A);
  gcu_tick(&st, &view);
  if (view.theme_index != 4 || view.sides_on != 0) {
    fprintf(stderr, "black theme sides should be off\n");
    return 1;
  }

  /* Button B play / pause */
  g_now = 0;
  gcu_init(&st, &g_hal);
  gcu_tick(&st, &view); /* clear boot riff one-shot */
  gcu_on_button(&st, GCU_BTN_B);
  gcu_tick(&st, &view);
  if (view.music != GCU_MUSIC_PLAYING || !view.request_play) {
    fprintf(stderr, "play request failed\n");
    return 1;
  }
  gcu_on_button(&st, GCU_BTN_B);
  gcu_tick(&st, &view);
  if (view.music != GCU_MUSIC_PAUSED || !view.request_pause) {
    fprintf(stderr, "pause request failed\n");
    return 1;
  }

  /* A while playing pauses without theme change */
  g_now = 0;
  gcu_init(&st, &g_hal);
  gcu_tick(&st, &view);
  gcu_on_button(&st, GCU_BTN_B);
  gcu_tick(&st, &view);
  int theme = view.theme_index;
  gcu_on_button(&st, GCU_BTN_A);
  gcu_tick(&st, &view);
  if (view.music != GCU_MUSIC_PAUSED || view.theme_index != theme) {
    fprintf(stderr, "A while playing should pause without theme change\n");
    return 1;
  }

  /* Details via C; second C no-op; A exits without theme change */
  gcu_on_button(&st, GCU_BTN_C);
  gcu_tick(&st, &view);
  if (view.screen != GCU_SCREEN_DETAILS) {
    fprintf(stderr, "C should open details\n");
    return 1;
  }
  theme = view.theme_index;
  gcu_on_button(&st, GCU_BTN_C);
  gcu_tick(&st, &view);
  if (view.screen != GCU_SCREEN_DETAILS) {
    fprintf(stderr, "second C should stay on details\n");
    return 1;
  }
  gcu_on_button(&st, GCU_BTN_A);
  gcu_tick(&st, &view);
  if (view.screen != GCU_SCREEN_FACE || view.theme_index != theme) {
    fprintf(stderr, "A on details should return face without theme change\n");
    return 1;
  }

  printf("OK defaults+domain\n");
  return 0;
}
