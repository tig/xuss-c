/* Device HAL backend — allowlisted for device headers. */
#include "hal_board.h"

#include "audio.h"
#include "display.h"
#include "leds.h"

#include "gcu/defaults.h"

#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static void set_led(gcu_hal_t *self, int on) {
  (void)self;
  /* Map plate LED to side strip pulse for visibility. */
  if (on) {
    leds_set_rgb(40, 140, 255);
  } else {
    leds_set_rgb(0, 0, 0);
  }
}

static void set_side_rgb(gcu_hal_t *self, uint8_t r, uint8_t g, uint8_t b) {
  (void)self;
  leds_set_rgb(r, g, b);
}

static void delay_ms(gcu_hal_t *self, int ms) {
  (void)self;
  vTaskDelay(pdMS_TO_TICKS(ms > 0 ? ms : 1));
}

static int64_t now_ms(gcu_hal_t *self) {
  (void)self;
  return esp_timer_get_time() / 1000;
}

static void fill_rect(gcu_hal_t *self, int x, int y, int w, int h,
                      uint16_t rgb565) {
  (void)self;
  display_fill_rect(x, y, w, h, rgb565);
}

static void blit(gcu_hal_t *self, int x, int y, int w, int h,
                 const uint16_t *pixels) {
  (void)self;
  display_blit(x, y, w, h, pixels);
}

static int btn_level(int gpio) {
  /* Active-low with external pull-ups on M5. */
  return gpio_get_level(gpio) == 0 ? 1 : 0;
}

static int btn_a(gcu_hal_t *self) {
  (void)self;
  return btn_level(GCU_PIN_BTN_A);
}
static int btn_b(gcu_hal_t *self) {
  (void)self;
  return btn_level(GCU_PIN_BTN_B);
}
static int btn_c(gcu_hal_t *self) {
  (void)self;
  return btn_level(GCU_PIN_BTN_C);
}

static int play_pcm(gcu_hal_t *self, const uint8_t *data, int len,
                    int sample_rate) {
  (void)self;
  return audio_play_pcm(data, len, sample_rate);
}

static int audio_busy_hal(gcu_hal_t *self) {
  (void)self;
  return audio_busy();
}

static void audio_stop_hal(gcu_hal_t *self) {
  (void)self;
  audio_stop();
}

static gcu_hal_t board_hal = {
    .set_led = set_led,
    .set_side_rgb = set_side_rgb,
    .delay_ms = delay_ms,
    .now_ms = now_ms,
    .fill_rect = fill_rect,
    .blit = blit,
    .btn_a = btn_a,
    .btn_b = btn_b,
    .btn_c = btn_c,
    .play_pcm = play_pcm,
    .audio_busy = audio_busy_hal,
    .audio_stop = audio_stop_hal,
};

gcu_hal_t *gcu_make_board_hal(void) {
  gpio_config_t io = {
      .mode = GPIO_MODE_INPUT,
      .pin_bit_mask = (1ULL << GCU_PIN_BTN_A) | (1ULL << GCU_PIN_BTN_B) |
                      (1ULL << GCU_PIN_BTN_C),
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
  };
  gpio_config(&io);

  (void)display_init();
  (void)leds_init();
  (void)audio_init();
  return &board_hal;
}
