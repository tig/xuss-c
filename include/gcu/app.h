#ifndef GCU_APP_H
#define GCU_APP_H

#include "gcu/music.h"
#include "gcu/themes.h"

/* Combined product state and the three-button map (§4.6). Pure logic so host
 * tests cover every context/button cell without hardware. */

typedef enum { GCU_SCREEN_FACE = 0, GCU_SCREEN_DETAILS } gcu_screen_t;

typedef enum { GCU_BTN_A = 0, GCU_BTN_B, GCU_BTN_C } gcu_button_t;

/* Commissioning params (§7.2). */
typedef struct {
  int mute;         /* 0/1 — blocks starting First */
  int volume;       /* fixed desk level */
  int telemetry_hz; /* 0 = off */
} gcu_config_t;

typedef struct {
  gcu_theme_t theme;
  gcu_screen_t screen;
  gcu_music_t music;
  gcu_config_t cfg;
} gcu_app_t;

void gcu_app_init(gcu_app_t *app);

/* Apply a debounced button edge per the §4.6 map. */
void gcu_app_button(gcu_app_t *app, gcu_button_t btn);

/* Playing cue (title on the living face) is shown while the track plays. */
int gcu_app_music_playing(const gcu_app_t *app);

#endif
