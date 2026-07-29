/* Speaker path: I2S → built-in DAC (GPIO25). More reliable for long streams
 * than dac_continuous on this board (descriptor timeouts under load). */
#include "audio.h"

#include "gcu/defaults.h"

#include "driver/i2s.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "audio";
static volatile int s_busy;
static volatile int s_pos;
static TaskHandle_t s_task;
static const uint8_t *s_data;
static int s_len;
static int s_rate;
static char s_path[64];
static int s_use_file;
static int s_start_off;
static int s_i2s_ready;

#define I2S_NUM I2S_NUM_0

static int i2s_open(int sample_rate) {
  if (sample_rate <= 0) {
    sample_rate = GCU_SAMPLE_RATE_HZ;
  }
  if (s_i2s_ready && s_rate == sample_rate) {
    return 0;
  }
  if (s_i2s_ready) {
    i2s_driver_uninstall(I2S_NUM);
    s_i2s_ready = 0;
  }
  i2s_config_t cfg = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN),
      .sample_rate = sample_rate,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT, /* DAC1 / GPIO25 */
      .communication_format = I2S_COMM_FORMAT_STAND_MSB,
      .intr_alloc_flags = 0,
      .dma_buf_count = 8,
      .dma_buf_len = 256,
      .use_apll = false,
      .tx_desc_auto_clear = true,
      .fixed_mclk = 0,
  };
  esp_err_t err = i2s_driver_install(I2S_NUM, &cfg, 0, NULL);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "i2s install: %s", esp_err_to_name(err));
    return -1;
  }
  err = i2s_set_dac_mode(I2S_DAC_CHANNEL_RIGHT_EN);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "i2s dac mode: %s", esp_err_to_name(err));
    i2s_driver_uninstall(I2S_NUM);
    return -1;
  }
  s_rate = sample_rate;
  s_i2s_ready = 1;
  ESP_LOGI(TAG, "I2S DAC ready GPIO25 @ %d Hz", sample_rate);
  return 0;
}

/* Expand u8 mono → 16-bit samples for I2S DAC (value in high byte). */
static size_t expand_u8(const uint8_t *src, int n, int16_t *dst) {
  for (int i = 0; i < n; i++) {
    dst[i] = (int16_t)(((uint16_t)src[i]) << 8);
  }
  return (size_t)n * sizeof(int16_t);
}

static void park_quiet(void) {
  if (!s_i2s_ready) {
    return;
  }
  int16_t mid[64];
  for (int i = 0; i < 64; i++) {
    mid[i] = (int16_t)(128 << 8);
  }
  size_t w = 0;
  (void)i2s_write(I2S_NUM, mid, sizeof mid, &w, pdMS_TO_TICKS(50));
  for (int i = 0; i < 64; i++) {
    mid[i] = 0;
  }
  (void)i2s_write(I2S_NUM, mid, sizeof mid, &w, pdMS_TO_TICKS(50));
}

static void audio_task(void *arg) {
  (void)arg;
  const int chunk = 512;
  uint8_t u8[512];
  int16_t s16[512];
  FILE *fp = NULL;
  int off = s_start_off;
  s_pos = off;

  if (s_use_file) {
    fp = fopen(s_path, "rb");
    if (!fp) {
      ESP_LOGE(TAG, "open failed %s", s_path);
      s_busy = 0;
      s_task = NULL;
      vTaskDelete(NULL);
      return;
    }
    if (off > 0) {
      fseek(fp, off, SEEK_SET);
    }
  }

  while (s_busy) {
    int n = 0;
    if (s_use_file) {
      n = (int)fread(u8, 1, (size_t)chunk, fp);
      if (n <= 0) {
        break;
      }
    } else {
      if (off >= s_len) {
        break;
      }
      n = s_len - off;
      if (n > chunk) {
        n = chunk;
      }
      memcpy(u8, s_data + off, (size_t)n);
    }
    size_t bytes = expand_u8(u8, n, s16);
    size_t written = 0;
    size_t done = 0;
    while (s_busy && done < bytes) {
      written = 0;
      esp_err_t err =
          i2s_write(I2S_NUM, ((uint8_t *)s16) + done, bytes - done, &written,
                    pdMS_TO_TICKS(500));
      if (err != ESP_OK) {
        ESP_LOGW(TAG, "i2s_write: %s", esp_err_to_name(err));
        s_busy = 0;
        break;
      }
      done += written;
    }
    off += n;
    s_pos = off;
  }

  if (fp) {
    fclose(fp);
  }
  park_quiet();
  s_busy = 0;
  s_task = NULL;
  vTaskDelete(NULL);
}

int audio_init(void) { return i2s_open(GCU_SAMPLE_RATE_HZ); }

int audio_play_pcm(const uint8_t *data, int len, int sample_rate) {
  if (!data || len <= 0 || s_busy) {
    return -1;
  }
  if (i2s_open(sample_rate) != 0) {
    return -1;
  }
  s_use_file = 0;
  s_data = data;
  s_len = len;
  s_start_off = 0;
  s_pos = 0;
  s_busy = 1;
  if (xTaskCreate(audio_task, "pcm", 4096, NULL, 6, &s_task) != pdPASS) {
    s_busy = 0;
    return -1;
  }
  return 0;
}

int audio_play_file(const char *path, int sample_rate, int start_offset) {
  if (!path || s_busy) {
    return -1;
  }
  FILE *probe = fopen(path, "rb");
  if (!probe) {
    ESP_LOGW(TAG, "missing %s", path);
    return -1;
  }
  fseek(probe, 0, SEEK_END);
  long sz = ftell(probe);
  fclose(probe);
  if (sz <= 0) {
    return -1;
  }
  if (start_offset < 0 || start_offset >= (int)sz) {
    start_offset = 0;
  }
  if (i2s_open(sample_rate) != 0) {
    return -1;
  }
  strncpy(s_path, path, sizeof s_path - 1);
  s_path[sizeof s_path - 1] = '\0';
  s_use_file = 1;
  s_data = NULL;
  s_len = (int)sz;
  s_start_off = start_offset;
  s_pos = start_offset;
  s_busy = 1;
  if (xTaskCreate(audio_task, "pcmfile", 4096, NULL, 6, &s_task) != pdPASS) {
    s_busy = 0;
    return -1;
  }
  ESP_LOGI(TAG, "stream %s off=%d size=%ld", path, start_offset, sz);
  return 0;
}

int audio_busy(void) { return s_busy; }

int audio_position(void) { return s_pos; }

void audio_stop(void) {
  if (!s_busy) {
    return;
  }
  s_busy = 0;
  vTaskDelay(pdMS_TO_TICKS(50));
}
