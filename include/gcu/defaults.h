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

typedef struct {
  int tick_sleep_ms;
  int side_led_pin;
  int side_led_count;
  int speaker_dac_pin;
  int tone_sample_hz;
  int tone_freq_hz;
  int tone_ms;
  int tone_amplitude;
} gcu_defaults_t;

/* Single shipped table (product path must use this, not parallel literals). */
extern const gcu_defaults_t GCU_DEFAULTS;

#endif
