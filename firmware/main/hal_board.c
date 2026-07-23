/* Device HAL backend — only TU allowlisted for device headers.
 * M5GO product face: side RGB strip (SK6812 on GPIO15) + speaker (DAC on
 * GPIO25). Pins come from the shipped defaults table, not local literals. */
#include "hal_board.h"

#include "gcu/defaults.h"

#include "driver/dac_continuous.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"

#include <math.h>
#include <stdlib.h>

static led_strip_handle_t strip;

static void set_led(gcu_hal_t *self, int on) {
  (void)self;
  if (!strip) {
    return;
  }
  if (on) {
    for (int i = 0; i < GCU_DEFAULTS.side_led_count; i++) {
      /* Default blue theme, modest brightness. */
      led_strip_set_pixel(strip, i, 0, 0, 64);
    }
    led_strip_refresh(strip);
  } else {
    led_strip_clear(strip);
  }
}

static void delay_ms(gcu_hal_t *self, int ms) {
  (void)self;
  vTaskDelay(pdMS_TO_TICKS(ms > 0 ? ms : 1));
}

static gcu_hal_t board_hal = {
    .set_led = set_led,
    .delay_ms = delay_ms,
};

gcu_hal_t *gcu_make_board_hal(void) {
  led_strip_config_t strip_cfg = {
      .strip_gpio_num = GCU_DEFAULTS.side_led_pin,
      .max_leds = (uint32_t)GCU_DEFAULTS.side_led_count,
      .led_model = LED_MODEL_SK6812,
      .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
  };
  led_strip_rmt_config_t rmt_cfg = {
      .resolution_hz = 10 * 1000 * 1000,
  };
  if (led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &strip) != ESP_OK) {
    strip = NULL; /* Face still runs; greeting tone and serial identity stay. */
  }
  return &board_hal;
}

/* Short boot tone: unsigned 8-bit mono PCM through the DAC (sample path;
 * never LEDC PWM on the speaker pin). Attack/release envelope so the tone
 * does not click into silence (spec.md §4.1). */
void gcu_board_boot_greeting(void) {
  const int rate = GCU_DEFAULTS.tone_sample_hz;
  const int total = rate * GCU_DEFAULTS.tone_ms / 1000;
  const int ramp = rate / 50; /* ~20 ms attack/release */
  const double amp = (double)GCU_DEFAULTS.tone_amplitude;
  const double step =
      2.0 * M_PI * (double)GCU_DEFAULTS.tone_freq_hz / (double)rate;

  uint8_t *buf = malloc((size_t)total);
  if (!buf) {
    return;
  }
  for (int i = 0; i < total; i++) {
    double env = 1.0;
    if (i < ramp) {
      env = (double)i / (double)ramp;
    } else if (i > total - ramp) {
      env = (double)(total - i) / (double)ramp;
    }
    buf[i] = (uint8_t)(128.0 + amp * env * sin(step * (double)i));
  }

  /* GPIO25 is DAC channel 0 on ESP32 (shipped speaker_dac_pin). */
  dac_continuous_handle_t dac = NULL;
  dac_continuous_config_t cfg = {
      .chan_mask = DAC_CHANNEL_MASK_CH0,
      .desc_num = 4,
      .buf_size = 2048,
      .freq_hz = (uint32_t)rate,
      .offset = 0,
      .clk_src = DAC_DIGI_CLK_SRC_DEFAULT,
      .chan_mode = DAC_CHANNEL_MODE_SIMUL,
  };
  if (dac_continuous_new_channels(&cfg, &dac) == ESP_OK) {
    if (dac_continuous_enable(dac) == ESP_OK) {
      size_t written = 0;
      dac_continuous_write(dac, buf, (size_t)total, &written, 5000);
      /* Let the DMA queue drain before teardown so the release ramp plays. */
      vTaskDelay(pdMS_TO_TICKS(120));
      dac_continuous_disable(dac);
    }
    dac_continuous_del_channels(dac);
  }
  free(buf);
}
