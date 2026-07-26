#ifndef GCU_DEFAULTS_H
#define GCU_DEFAULTS_H

#include <stdint.h>

/* Shipped product defaults — metal and host tests share this table. */

/* --- M5GO pin pack (silico board-profile m5go; confirm on unit) --- */
#define GCU_PIN_SIDE_LED 15
#define GCU_PIN_SPEAKER 25
#define GCU_PIN_DISPLAY_SCLK 18
#define GCU_PIN_DISPLAY_MOSI 23
#define GCU_PIN_DISPLAY_CS 14
#define GCU_PIN_DISPLAY_DC 27
#define GCU_PIN_DISPLAY_RST 33
#define GCU_PIN_DISPLAY_BL 32
#define GCU_PIN_BTN_A 39
#define GCU_PIN_BTN_B 38
#define GCU_PIN_BTN_C 37
#define GCU_PIN_I2C_SDA 21
#define GCU_PIN_I2C_SCL 22

#define GCU_SIDE_LED_COUNT 10
#define GCU_AUDIO_SAMPLE_HZ 22050
#define GCU_DISPLAY_W 320
#define GCU_DISPLAY_H 240

#define GCU_WINK_PERIOD_MS 10000
#define GCU_WINK_HOLD_MS 180
#define GCU_BANNER_STEP_MS 40
#define GCU_BTN_DEBOUNCE_MS 40
#define GCU_UI_TICK_MS 20
#define GCU_DETAILS_REFRESH_MS 100

#define GCU_BANNER_TEXT "Xuss-C; built on ESP-IDF"
#define GCU_PLAYING_CUE "First by Tig"

/* Theme cycle: blue → orange → red → green → black → blue … */
#define GCU_THEME_COUNT 5

typedef struct {
  uint8_t r, g, b;
} gcu_rgb_t;

typedef struct {
  gcu_rgb_t face;
  gcu_rgb_t bg;
  gcu_rgb_t banner_ink;
  int sides_on; /* 0 for black theme */
} gcu_theme_t;

typedef struct {
  int wink_period_ms;
  int wink_hold_ms;
  int banner_step_ms;
  int btn_debounce_ms;
  int ui_tick_ms;
  int details_refresh_ms;
  int audio_sample_hz;
  int side_led_count;
  int display_w;
  int display_h;
  int pin_side_led;
  int pin_speaker;
  int pin_btn_a;
  int pin_btn_b;
  int pin_btn_c;
} gcu_defaults_t;

extern const gcu_defaults_t GCU_DEFAULTS;
extern const gcu_theme_t GCU_THEMES[GCU_THEME_COUNT];

#endif
