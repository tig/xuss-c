/* Speaker DAC path — sample PCM only (no LEDC on GPIO25). */
#include "audio.h"

#include "gcu/defaults.h"

#include "driver/dac_continuous.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

static const char *TAG = "audio";
static dac_continuous_handle_t s_dac;
static volatile int s_busy;
static TaskHandle_t s_task;
static const uint8_t *s_data;
static int s_len;
static int s_rate;

static void audio_task(void *arg) {
  (void)arg;
  const int chunk = 512;
  int off = 0;
  while (s_busy && off < s_len) {
    int n = s_len - off;
    if (n > chunk) {
      n = chunk;
    }
    size_t written = 0;
    esp_err_t err =
        dac_continuous_write(s_dac, (uint8_t *)(s_data + off), (size_t)n,
                             &written, pdMS_TO_TICKS(200));
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "dac write: %s", esp_err_to_name(err));
      break;
    }
    off += (int)written;
  }
  /* Soft park to mid/low to reduce click. */
  uint8_t park[32];
  memset(park, 128, sizeof park);
  size_t w = 0;
  (void)dac_continuous_write(s_dac, park, sizeof park, &w, pdMS_TO_TICKS(50));
  memset(park, 0, sizeof park);
  (void)dac_continuous_write(s_dac, park, sizeof park, &w, pdMS_TO_TICKS(50));
  s_busy = 0;
  s_task = NULL;
  vTaskDelete(NULL);
}

int audio_init(void) {
  if (s_dac) {
    return 0;
  }
  dac_continuous_config_t cfg = {
      .chan_mask = DAC_CHANNEL_MASK_CH0, /* GPIO25 */
      .desc_num = 4,
      .buf_size = 2048,
      .freq_hz = GCU_SAMPLE_RATE_HZ,
      .offset = 0,
      .clk_src = DAC_DIGI_CLK_SRC_DEFAULT,
      .chan_mode = DAC_CHANNEL_MODE_SIMUL,
  };
  ESP_RETURN_ON_ERROR(dac_continuous_new_channels(&cfg, &s_dac), TAG, "new");
  ESP_RETURN_ON_ERROR(dac_continuous_enable(s_dac), TAG, "enable");
  ESP_LOGI(TAG, "DAC continuous ready on GPIO%d @ %d Hz", GCU_PIN_SPEAKER,
           GCU_SAMPLE_RATE_HZ);
  return 0;
}

int audio_play_pcm(const uint8_t *data, int len, int sample_rate) {
  if (!s_dac || !data || len <= 0 || s_busy) {
    return -1;
  }
  if (sample_rate > 0 && sample_rate != s_rate) {
    /* Reconfigure frequency if needed — disable/enable channel. */
    dac_continuous_disable(s_dac);
    dac_continuous_del_channels(s_dac);
    s_dac = NULL;
    dac_continuous_config_t cfg = {
        .chan_mask = DAC_CHANNEL_MASK_CH0,
        .desc_num = 4,
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
  }
  s_data = data;
  s_len = len;
  s_busy = 1;
  if (xTaskCreate(audio_task, "pcm", 3072, NULL, 5, &s_task) != pdPASS) {
    s_busy = 0;
    return -1;
  }
  return 0;
}

int audio_busy(void) { return s_busy; }

void audio_stop(void) {
  if (!s_busy) {
    return;
  }
  s_busy = 0;
  /* Task observes flag and exits; brief wait. */
  vTaskDelay(pdMS_TO_TICKS(30));
}
