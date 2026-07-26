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

/* Logical RGB — panel pack + SK6812 share these triples after measure. */
const gcu_theme_t GCU_THEMES[GCU_THEME_COUNT] = {
    /* blue (default) */
    {.face = {40, 140, 255},
     .bg = {8, 16, 48},
     .banner_ink = {200, 230, 255},
     .sides_on = 1},
    /* orange */
    {.face = {255, 140, 40},
     .bg = {40, 20, 8},
     .banner_ink = {255, 220, 180},
     .sides_on = 1},
    /* red */
    {.face = {255, 48, 48},
     .bg = {40, 8, 8},
     .banner_ink = {255, 200, 200},
     .sides_on = 1},
    /* green */
    {.face = {48, 220, 96},
     .bg = {8, 32, 16},
     .banner_ink = {200, 255, 210},
     .sides_on = 1},
    /* black: black face on white bg; sides off */
    {.face = {0, 0, 0},
     .bg = {255, 255, 255},
     .banner_ink = {32, 32, 32},
     .sides_on = 0},
};
