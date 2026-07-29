#ifndef GCU_DEFAULTS_H
#define GCU_DEFAULTS_H

/* Shipped product defaults — metal and host tests share this table. */

#define GCU_TICK_SLEEP_MS 16
#define GCU_WINK_PERIOD_MS 10000
#define GCU_WINK_HOLD_MS 180
#define GCU_BANNER_STEP_MS 20
#define GCU_DEBOUNCE_MS 40
#define GCU_SAMPLE_RATE_HZ 22050

/* M5GO pin pack (board-profile m5go candidates). */
#define GCU_PIN_SIDE_LED 15
#define GCU_PIN_SPEAKER 25
#define GCU_PIN_BTN_A 39
#define GCU_PIN_BTN_B 38
#define GCU_PIN_BTN_C 37
#define GCU_PIN_LCD_SCLK 18
#define GCU_PIN_LCD_MOSI 23
#define GCU_PIN_LCD_CS 14
#define GCU_PIN_LCD_DC 27
#define GCU_PIN_LCD_RST 33
#define GCU_PIN_LCD_BL 32
#define GCU_PIN_I2C_SDA 21
#define GCU_PIN_I2C_SCL 22

#define GCU_SIDE_LED_COUNT 10
#define GCU_LCD_W 320
#define GCU_LCD_H 240

#define GCU_BANNER_TEXT "Xuss-C; built on ESP-IDF"

typedef struct {
  int tick_sleep_ms;
  int wink_period_ms;
  int wink_hold_ms;
  int banner_step_ms;
  int debounce_ms;
  int sample_rate_hz;
} gcu_defaults_t;

/* Single shipped table (product path must use this, not parallel literals). */
extern const gcu_defaults_t GCU_DEFAULTS;

#endif
