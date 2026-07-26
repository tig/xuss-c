#include "gcu/defaults.h"

const gcu_defaults_t GCU_DEFAULTS = {
    .wink_period_ms = GCU_WINK_PERIOD_MS,
    .wink_hold_ms = GCU_WINK_HOLD_MS,
    .banner_step_ms = GCU_BANNER_STEP_MS,
    .btn_debounce_ms = GCU_BTN_DEBOUNCE_MS,
    .ui_tick_ms = GCU_UI_TICK_MS,
    .details_refresh_ms = GCU_DETAILS_REFRESH_MS,
    .audio_sample_hz = GCU_AUDIO_SAMPLE_HZ,
    .side_led_count = GCU_SIDE_LED_COUNT,
    .display_w = GCU_DISPLAY_W,
    .display_h = GCU_DISPLAY_H,
    .pin_side_led = GCU_PIN_SIDE_LED,
    .pin_speaker = GCU_PIN_SPEAKER,
    .pin_btn_a = GCU_PIN_BTN_A,
    .pin_btn_b = GCU_PIN_BTN_B,
    .pin_btn_c = GCU_PIN_BTN_C,
};

/* Logical RGB — same triples for IPS face and SK6812 sides (after pack fix). */
const gcu_theme_t GCU_THEMES[GCU_THEME_COUNT] = {
    /* blue (default) — strong primary for LED/panel match checks */
    {.face = {0, 96, 255},
     .bg = {0, 16, 48},
     .banner_ink = {180, 220, 255},
     .sides_on = 1},
    /* orange */
    {.face = {255, 128, 0},
     .bg = {40, 16, 0},
     .banner_ink = {255, 210, 160},
     .sides_on = 1},
    /* red */
    {.face = {255, 0, 0},
     .bg = {40, 0, 0},
     .banner_ink = {255, 180, 180},
     .sides_on = 1},
    /* green */
    {.face = {0, 220, 64},
     .bg = {0, 32, 8},
     .banner_ink = {180, 255, 200},
     .sides_on = 1},
    /* black: black face on white bg; sides off */
    {.face = {0, 0, 0},
     .bg = {255, 255, 255},
     .banner_ink = {32, 32, 32},
     .sides_on = 0},
};
