/* Device HAL backend — only TU allowlisted for device headers. */
#include "hal_board.h"
#include "hal_imu.h"
#include "hal_input.h"
#include "hal_leds.h"

#include "driver/dac_continuous.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdint.h>

#ifndef GCU_LED_GPIO
#define GCU_LED_GPIO 2
#endif

static void set_led(gcu_hal_t *self, int on) {
  (void)self;
  gpio_set_level(GCU_LED_GPIO, on ? 1 : 0);
}

static void delay_ms(gcu_hal_t *self, int ms) {
  (void)self;
  vTaskDelay(pdMS_TO_TICKS(ms > 0 ? ms : 1));
}

static long now_ms(gcu_hal_t *self) {
  (void)self;
  return (long)(esp_timer_get_time() / 1000);
}

static long free_heap(gcu_hal_t *self) {
  (void)self;
  return (long)heap_caps_get_free_size(MALLOC_CAP_8BIT);
}

/*
 * Speaker audio (spec §3): unsigned 8-bit mono PCM out the built-in DAC on
 * GPIO25 (DAC channel 0 = M5GO speaker), DMA-buffered via dac_continuous.
 * No LEDC/PWM. APLL clock keeps arbitrary rates (e.g. 22050 Hz) accurate.
 * The handle is created lazily and reused; re-created if the rate changes.
 */
static dac_continuous_handle_t s_dac;
static int s_dac_rate;

static void audio_ensure(int sample_rate_hz) {
  if (s_dac && s_dac_rate == sample_rate_hz) {
    return;
  }
  if (s_dac) {
    dac_continuous_disable(s_dac);
    dac_continuous_del_channels(s_dac);
    s_dac = NULL;
    s_dac_rate = 0;
  }
  dac_continuous_config_t cfg = {
      .chan_mask = DAC_CHANNEL_MASK_CH0, /* GPIO25 — M5GO speaker */
      .desc_num = 4,
      .buf_size = 2048,
      .freq_hz = (uint32_t)sample_rate_hz,
      .offset = 0,
      .clk_src = DAC_DIGI_CLK_SRC_APLL,
      .chan_mode = DAC_CHANNEL_MODE_SIMUL,
  };
  if (dac_continuous_new_channels(&cfg, &s_dac) != ESP_OK) {
    s_dac = NULL;
    return;
  }
  if (dac_continuous_enable(s_dac) != ESP_OK) {
    dac_continuous_del_channels(s_dac);
    s_dac = NULL;
    return;
  }
  s_dac_rate = sample_rate_hz;
}

static void play_pcm(gcu_hal_t *self, const unsigned char *pcm, int len,
                     int sample_rate_hz) {
  (void)self;
  if (!pcm || len <= 0 || sample_rate_hz <= 0) {
    return;
  }
  audio_ensure(sample_rate_hz);
  if (!s_dac) {
    return;
  }
  size_t off = 0;
  while (off < (size_t)len) {
    size_t loaded = 0;
    if (dac_continuous_write(s_dac, (uint8_t *)pcm + off, (size_t)len - off,
                             &loaded, 1000) != ESP_OK) {
      break;
    }
    if (loaded == 0) {
      break;
    }
    off += loaded;
  }
}

static gcu_hal_t board_hal = {
    .set_led = set_led,
    .delay_ms = delay_ms,
    .now_ms = now_ms,
    .read_buttons = gcu_input_read,
    .read_imu = gcu_imu_read,
    .free_heap = free_heap,
    .set_leds = gcu_leds_set,
    .play_pcm = play_pcm,
};

gcu_hal_t *gcu_make_board_hal(void) {
  gpio_reset_pin(GCU_LED_GPIO);
  gpio_set_direction(GCU_LED_GPIO, GPIO_MODE_OUTPUT);
  gcu_input_init();
  gcu_imu_init();
  gcu_leds_init();
  return &board_hal;
}
