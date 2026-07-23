#ifndef GCU_DEFAULTS_H
#define GCU_DEFAULTS_H

/* Shipped product defaults — metal and host tests share this table. */

#define GCU_TICK_SLEEP_MS 250

/* M5GO product face (board profile m5go, operator-confirmed):
 * side RGB strip on GPIO15 (10x SK6812), speaker on GPIO25 (ESP32 DAC1). */
#define GCU_SIDE_LED_PIN 15
#define GCU_SIDE_LED_COUNT 10
#define GCU_SPEAKER_DAC_PIN 25

/* Boot greeting tone: sample PCM on the DAC path (no LEDC PWM on the
 * speaker pin — spec.md §3). 441 Hz gives an exact 50-sample cycle at
 * the 22050 Hz project rate. */
#define GCU_TONE_SAMPLE_HZ 22050
#define GCU_TONE_FREQ_HZ 441
#define GCU_TONE_MS 800
#define GCU_TONE_AMPLITUDE 40 /* of 127; gentle desk level */

/* Front buttons (M5Stack Core: A=GPIO39, B=GPIO38, C=GPIO37, active low;
 * no internal pulls on 34-39 — board has external pullups). */
#define GCU_BTN_A_PIN 39
#define GCU_BTN_B_PIN 38
#define GCU_BTN_C_PIN 37
#define GCU_DEBOUNCE_MS 20

/* Internal I2C bus (MPU6886 IMU on the M5 core). */
#define GCU_I2C_SDA_PIN 21
#define GCU_I2C_SCL_PIN 22

/* UI schedule (spec.md §4.2, §4.5). Banner step and wink hold are not
 * pinned by the spec ("smooth scroll", "short wink") — chosen here, logged
 * as assumptions on issue #4. */
#define GCU_WINK_PERIOD_MS 10000
#define GCU_WINK_HOLD_MS 300
#define GCU_BANNER_STEP_MS 30
#define GCU_DETAILS_REFRESH_MS 100
#define GCU_VOLUME_DEFAULT 40 /* of 127; comfortable desk level */

typedef struct {
  int tick_sleep_ms;
  int side_led_pin;
  int side_led_count;
  int speaker_dac_pin;
  int tone_sample_hz;
  int tone_freq_hz;
  int tone_ms;
  int tone_amplitude;
  int btn_a_pin;
  int btn_b_pin;
  int btn_c_pin;
  int debounce_ms;
  int i2c_sda_pin;
  int i2c_scl_pin;
  int wink_period_ms;
  int wink_hold_ms;
  int banner_step_ms;
  int details_refresh_ms;
  int volume_default;
} gcu_defaults_t;

/* Single shipped table (product path must use this, not parallel literals). */
extern const gcu_defaults_t GCU_DEFAULTS;

#endif
