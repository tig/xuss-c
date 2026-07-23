#include "gcu/defaults.h"

const gcu_defaults_t GCU_DEFAULTS = {
    .tick_sleep_ms = GCU_TICK_SLEEP_MS,
    .side_led_pin = GCU_SIDE_LED_PIN,
    .side_led_count = GCU_SIDE_LED_COUNT,
    .speaker_dac_pin = GCU_SPEAKER_DAC_PIN,
    .tone_sample_hz = GCU_TONE_SAMPLE_HZ,
    .tone_freq_hz = GCU_TONE_FREQ_HZ,
    .tone_ms = GCU_TONE_MS,
    .tone_amplitude = GCU_TONE_AMPLITUDE,
    .btn_a_pin = GCU_BTN_A_PIN,
    .btn_b_pin = GCU_BTN_B_PIN,
    .btn_c_pin = GCU_BTN_C_PIN,
    .debounce_ms = GCU_DEBOUNCE_MS,
    .i2c_sda_pin = GCU_I2C_SDA_PIN,
    .i2c_scl_pin = GCU_I2C_SCL_PIN,
    .wink_period_ms = GCU_WINK_PERIOD_MS,
    .wink_hold_ms = GCU_WINK_HOLD_MS,
    .banner_step_ms = GCU_BANNER_STEP_MS,
    .details_refresh_ms = GCU_DETAILS_REFRESH_MS,
    .volume_default = GCU_VOLUME_DEFAULT,
};
