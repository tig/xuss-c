/* Speaker DAC path — sample PCM only (no LEDC on GPIO25). */
#include "audio.h"

#include "gcu/defaults.h"

#include "driver/dac_continuous.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "audio";
static dac_continuous_handle_t s_dac;
static volatile int s_busy;
static volatile int s_pos;
static TaskHandle_t s_task;
static const uint8_t *s_data;
static int s_len;
static int s_rate;
static char s_path[64];
static int s_use_file;
static int s_start_off;

static int ensure_rate(int sample_rate) {
  if (sample_rate <= 0) {
    sample_rate = GCU_SAMPLE_RATE_HZ;
  }
  if (s_dac && sample_rate == s_rate) {
    return 0;
  }
  if (s_dac) {
    dac_continuous_disable(s_dac);
    dac_continuous_del_channels(s_dac);
    s_dac = NULL;
  }
  dac_continuous_config_t cfg = {
      .chan_mask = DAC_CHANNEL_MASK_CH0,
      .desc_num = 8,
      .buf_size = 2048,
      .freq_hz = sample_rate,
      .offset = 0,
      .clk_src = DAC_DIGI_CLK_SRC_DEFAULT,
      .chan_mode = DAC_CHANNEL_MODE_SIMUL,
  };
  if (dac_continuous_new_channels(&cfg, &s_dac) != ESP_OK ||
      dac_continuous_enable(s_dac) != ESP_OK) {
    s_dac = NULL;
    return -1;
  }
  s_rate = sample_rate;
  return 0;
}

static void park_dac(void) {
  if (!s_dac) {
    return;
  }
  uint8_t park[32];
  memset(park, 128, sizeof park);
  size_t w = 0;
  (void)dac_continuous_write(s_dac, park, sizeof park, &w, pdMS_TO_TICKS(40));
  memset(park, 0, sizeof park);
  (void)dac_continuous_write(s_dac, park, sizeof park, &w, pdMS_TO_TICKS(40));
}

static void audio_task(void *arg) {
  (void)arg;
  const int chunk = 1024;
  uint8_t buf[1024];
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
      n = (int)fread(buf, 1, (size_t)chunk, fp);
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
      memcpy(buf, s_data + off, (size_t)n);
    }
    size_t written = 0;
    size_t done = 0;
    while (s_busy && done < (size_t)n) {
      written = 0;
      esp_err_t err = dac_continuous_write(s_dac, buf + done, (size_t)n - done,
                                           &written, pdMS_TO_TICKS(200));
      if (err != ESP_OK) {
        ESP_LOGW(TAG, "dac write: %s", esp_err_to_name(err));
        s_busy = 0;
        break;
      }
      done += written;
      off += (int)written;
      s_pos = off;
    }
  }

  if (fp) {
    fclose(fp);
  }
  park_dac();
  s_busy = 0;
  s_task = NULL;
  vTaskDelete(NULL);
}

int audio_init(void) {
  return ensure_rate(GCU_SAMPLE_RATE_HZ);
}

int audio_play_pcm(const uint8_t *data, int len, int sample_rate) {
  if (!data || len <= 0 || s_busy) {
    return -1;
  }
  if (ensure_rate(sample_rate) != 0) {
    return -1;
  }
  s_use_file = 0;
  s_data = data;
  s_len = len;
  s_start_off = 0;
  s_pos = 0;
  s_busy = 1;
  if (xTaskCreate(audio_task, "pcm", 4096, NULL, 5, &s_task) != pdPASS) {
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
  if (ensure_rate(sample_rate) != 0) {
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
  if (xTaskCreate(audio_task, "pcmfile", 4096, NULL, 5, &s_task) != pdPASS) {
    s_busy = 0;
    return -1;
  }
  return 0;
}

int audio_busy(void) { return s_busy; }

int audio_position(void) { return s_pos; }

void audio_stop(void) {
  if (!s_busy) {
    return;
  }
  s_busy = 0;
  vTaskDelay(pdMS_TO_TICKS(40));
}
