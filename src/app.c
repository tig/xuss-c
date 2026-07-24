#include "gcu/app.h"
#include "gcu/defaults.h"

void gcu_app_init(gcu_app_t *app) {
  if (!app) {
    return;
  }
  app->theme = GCU_THEME_BLUE;
  app->screen = GCU_SCREEN_FACE;
  gcu_music_init(&app->music);
  app->cfg.mute = GCU_DEFAULTS.mute;
  app->cfg.volume = GCU_DEFAULTS.volume;
  app->cfg.telemetry_hz = 0;
}

void gcu_app_button(gcu_app_t *app, gcu_button_t btn) {
  if (!app) {
    return;
  }

  switch (btn) {
  case GCU_BTN_A:
    /* Left: on Details, only exit to the face (never advances the theme). On
     * the face, cycle the theme — including while music plays. NOTE: this is an
     * operator-requested deviation from spec §4.3/§4.6/§8, which say A pauses
     * while playing. Recorded in the PR ambiguity log. */
    if (app->screen == GCU_SCREEN_DETAILS) {
      app->screen = GCU_SCREEN_FACE; /* music state unchanged */
    } else {
      app->theme = gcu_theme_next(app->theme);
    }
    break;

  case GCU_BTN_B:
    /* Middle: play/pause/resume in any screen; screen is unchanged (§4.4). */
    gcu_music_press(&app->music, app->cfg.mute);
    break;

  case GCU_BTN_C:
    /* Right: open Details from the face without pausing; no-op on Details. */
    if (app->screen == GCU_SCREEN_FACE) {
      app->screen = GCU_SCREEN_DETAILS; /* music continues */
    }
    break;
  }
}

int gcu_app_music_playing(const gcu_app_t *app) {
  return app && gcu_music_is_playing(&app->music);
}
