/* Device HAL backend — allowlisted for device headers. */
#include "hal_board.h"

#include "audio.h"
#include "display.h"
#include "imu.h"
#include "leds.h"
#include "storage.h"

#include "gcu/defaults.h"

#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

static void set_led(gcu_hal_t *self, int on) {
  (void)self;
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

static int btn_level(int gpio) { return gpio_get_level(gpio) == 0 ? 1 : 0; }

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

static int play_file(gcu_hal_t *self, const char *path, int sample_rate,
                     int start_offset) {
  (void)self;
  return audio_play_file(path, sample_rate, start_offset);
}

static int audio_busy_hal(gcu_hal_t *self) {
  (void)self;
  return audio_busy();
}

static int audio_position_hal(gcu_hal_t *self) {
  (void)self;
  return audio_position();
}

static void audio_stop_hal(gcu_hal_t *self) {
  (void)self;
  audio_stop();
}

static int read_sensors(gcu_hal_t *self, gcu_sensors_t *out) {
  (void)self;
  if (!out) {
    return -1;
  }
  imu_sample_t s;
  memset(out, 0, sizeof(*out));
  out->btn_a = btn_level(GCU_PIN_BTN_A);
  out->btn_b = btn_level(GCU_PIN_BTN_B);
  out->btn_c = btn_level(GCU_PIN_BTN_C);
  out->heap_free = (int)heap_caps_get_free_size(MALLOC_CAP_8BIT);
  if (imu_read(&s) == 0 && s.present) {
    out->present = 1;
    out->ax = s.ax;
    out->ay = s.ay;
    out->az = s.az;
    out->gx = s.gx;
    out->gy = s.gy;
    out->gz = s.gz;
    out->temp_c = s.temp_c;
  }
  return 0;
}

static int song_size(gcu_hal_t *self) {
  (void)self;
  return (int)storage_first_pcm_size();
}

static const char *song_path(gcu_hal_t *self) {
  (void)self;
  return storage_first_pcm_path();
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
    .play_file = play_file,
    .audio_busy = audio_busy_hal,
    .audio_position = audio_position_hal,
    .audio_stop = audio_stop_hal,
    .read_sensors = read_sensors,
    .song_size = song_size,
    .song_path = song_path,
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
  (void)storage_init();
  (void)imu_init();
  return &board_hal;
}
