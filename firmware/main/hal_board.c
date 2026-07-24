/* Device HAL backend — only TU allowlisted for device headers. */
#include "hal_board.h"
#include "hal_imu.h"
#include "hal_input.h"
#include "hal_leds.h"

#include "driver/dac_continuous.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

/*
 * Streaming full-song playback (§4.4, §5.2): a dedicated FreeRTOS task reads
 * u8 PCM from SPIFFS in small chunks and feeds the DAC. dac_continuous_write
 * blocks on the DMA queue, which paces the task without stalling the UI/link
 * task. Play/pause/resume is start-at-offset + stop; natural EOF sets `done`.
 */
static TaskHandle_t s_audio_task;
static volatile long s_audio_pos;   /* samples consumed */
static volatile int s_audio_stop;   /* request task exit */
static volatile int s_audio_done;   /* reached EOF */
static char s_audio_path[64];
static int s_audio_rate;
static long s_audio_start;

static void audio_stream_task(void *arg) {
  (void)arg;
  FILE *f = fopen(s_audio_path, "rb");
  if (f) {
    fseek(f, s_audio_start, SEEK_SET);
    audio_ensure(s_audio_rate);
    long pos = s_audio_start;
    static uint8_t buf[2048];
    while (!s_audio_stop && s_dac) {
      size_t n = fread(buf, 1, sizeof buf, f);
      if (n == 0) {
        s_audio_done = 1;
        break;
      }
      size_t off = 0;
      while (off < n && !s_audio_stop) {
        size_t loaded = 0;
        if (dac_continuous_write(s_dac, buf + off, n - off, &loaded, 1000) !=
            ESP_OK) {
          break;
        }
        off += loaded;
      }
      pos += (long)n;
      s_audio_pos = pos;
    }
    fclose(f);
  } else {
    s_audio_done = 1;
  }
  s_audio_task = NULL;
  vTaskDelete(NULL);
}

static void music_stop(gcu_hal_t *self) {
  (void)self;
  if (s_audio_task) {
    s_audio_stop = 1;
    while (s_audio_task) {
      vTaskDelay(pdMS_TO_TICKS(2));
    }
  }
}

static void music_start(gcu_hal_t *self, const char *path, int sample_rate_hz,
                        long start_sample) {
  if (!path || sample_rate_hz <= 0) {
    return;
  }
  music_stop(self);
  strncpy(s_audio_path, path, sizeof s_audio_path - 1);
  s_audio_path[sizeof s_audio_path - 1] = '\0';
  s_audio_rate = sample_rate_hz;
  s_audio_start = start_sample < 0 ? 0 : start_sample;
  s_audio_pos = s_audio_start;
  s_audio_stop = 0;
  s_audio_done = 0;
  xTaskCreate(audio_stream_task, "audio", 4096, NULL, 5, &s_audio_task);
}

static long music_pos(gcu_hal_t *self) {
  (void)self;
  return s_audio_pos;
}

static int music_done(gcu_hal_t *self) {
  (void)self;
  return s_audio_done;
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
    .music_start = music_start,
    .music_stop = music_stop,
    .music_pos = music_pos,
    .music_done = music_done,
};

static void spiffs_mount(void) {
  esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = "storage",
      .max_files = 2,
      .format_if_mount_failed = false,
  };
  esp_err_t err = esp_vfs_spiffs_register(&conf);
  if (err != ESP_OK) {
    ESP_LOGW("hal_board", "SPIFFS mount failed: %s", esp_err_to_name(err));
  }
}

gcu_hal_t *gcu_make_board_hal(void) {
  gpio_reset_pin(GCU_LED_GPIO);
  gpio_set_direction(GCU_LED_GPIO, GPIO_MODE_OUTPUT);
  gcu_input_init();
  gcu_imu_init();
  gcu_leds_init();
  spiffs_mount();
  return &board_hal;
}
