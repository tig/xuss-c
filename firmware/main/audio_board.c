/* Device audio backend — allowlisted for device headers.
 * DAC path only: unsigned 8-bit mono PCM via dac_continuous DMA
 * (never LEDC PWM on the speaker pin — spec.md §3). */
#include "audio_board.h"

#include "gcu/audio.h"
#include "gcu/defaults.h"
#include "gcu/ui.h" /* gcu_audio_req_t request values */

#include "driver/dac_continuous.h"
#include "esp_partition.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include <stdio.h>
#include <string.h>

/* Boot riff: seconds ~7.5–10 of First, embedded in the app image. */
extern const uint8_t riff_start[] asm("_binary_riff_pcm_start");
extern const uint8_t riff_end[] asm("_binary_riff_pcm_end");

#define AUDIO_HDR_LEN 16
static const char AUDIO_MAGIC[4] = {'X', 'C', 'A', '1'};

static const esp_partition_t *audio_part;
static unsigned audio_pcm_len; /* 0 = missing/invalid (refuse playback) */

/* Blocking DAC pump for a PLAYING engine; returns at pause/end/error. */
static void dac_pump(gcu_audio_t *eng, int rate) {
  dac_continuous_handle_t dac = NULL;
  dac_continuous_config_t cfg = {
      .chan_mask = DAC_CHANNEL_MASK_CH0, /* GPIO25: shipped speaker pin */
      .desc_num = 4,
      .buf_size = 2048,
      .freq_hz = (uint32_t)rate,
      .offset = 0,
      .clk_src = DAC_DIGI_CLK_SRC_DEFAULT,
      .chan_mode = DAC_CHANNEL_MODE_SIMUL,
  };
  if (dac_continuous_new_channels(&cfg, &dac) != ESP_OK) {
    return;
  }
  if (dac_continuous_enable(dac) == ESP_OK) {
    static uint8_t chunk[2048];
    int n;
    while ((n = gcu_audio_next_chunk(eng, chunk, (int)sizeof chunk)) > 0) {
      size_t written = 0;
      dac_continuous_write(dac, chunk, (size_t)n, &written, 1000);
    }
    /* Let the DMA queue drain so the tail (fade/last samples) plays. */
    vTaskDelay(pdMS_TO_TICKS(120));
    dac_continuous_disable(dac);
  }
  dac_continuous_del_channels(dac);
}

static int riff_read(void *user, unsigned offset, unsigned char *dst, int n) {
  (void)user;
  memcpy(dst, riff_start + offset, (size_t)n);
  return n;
}

void gcu_board_boot_greeting(void) {
  gcu_audio_t eng;
  unsigned len = (unsigned)(riff_end - riff_start);
  gcu_audio_init(&eng, riff_read, NULL, len, GCU_DEFAULTS.volume_default);
  if (gcu_audio_start(&eng)) {
    dac_pump(&eng, GCU_DEFAULTS.tone_sample_hz);
  }
}

/* --- Full-song playback task (spec §5.2: audio never blocks UI/link) --- */

static QueueHandle_t req_q;
static QueueHandle_t ev_q;
static gcu_audio_t song;

static void audio_task(void *arg) {
  (void)arg;
  dac_continuous_handle_t dac = NULL;
  dac_continuous_config_t cfg = {
      .chan_mask = DAC_CHANNEL_MASK_CH0,
      .desc_num = 4,
      .buf_size = 2048,
      .freq_hz = (uint32_t)GCU_DEFAULTS.tone_sample_hz,
      .offset = 0,
      .clk_src = DAC_DIGI_CLK_SRC_DEFAULT,
      .chan_mode = DAC_CHANNEL_MODE_SIMUL,
  };
  static uint8_t chunk[2048];
  int dac_on = 0;

  for (;;) {
    int req;
    TickType_t wait = song.state == GCU_AUDIO_PLAYING ? 0 : pdMS_TO_TICKS(50);
    while (xQueueReceive(req_q, &req, wait) == pdTRUE) {
      switch (req) {
        case GCU_AUDIO_START:
          gcu_audio_start(&song);
          break;
        case GCU_AUDIO_PAUSE:
          gcu_audio_pause(&song);
          break;
        case GCU_AUDIO_RESUME:
          gcu_audio_resume(&song);
          break;
        default: /* park/stop (repl, reboot) */
          gcu_audio_stop(&song);
          break;
      }
      wait = 0;
    }

    if (song.state == GCU_AUDIO_PLAYING) {
      if (!dac_on) {
        if (dac_continuous_new_channels(&cfg, &dac) != ESP_OK ||
            dac_continuous_enable(dac) != ESP_OK) {
          gcu_audio_stop(&song);
          int ev = 2; /* error */
          xQueueSend(ev_q, &ev, 0);
          continue;
        }
        dac_on = 1;
      }
      int n = gcu_audio_next_chunk(&song, chunk, (int)sizeof chunk);
      if (n > 0) {
        size_t written = 0;
        dac_continuous_write(dac, chunk, (size_t)n, &written, 1000);
      } else {
        /* Natural end (engine already reset) — tell the UI loop. */
        int ev = 1;
        xQueueSend(ev_q, &ev, 0);
      }
    }
    if (song.state != GCU_AUDIO_PLAYING && dac_on) {
      vTaskDelay(pdMS_TO_TICKS(120)); /* drain so pause does not click */
      dac_continuous_disable(dac);
      dac_continuous_del_channels(dac);
      dac = NULL;
      dac_on = 0;
    }
  }
}

void gcu_board_audio_task_start(unsigned pcm_len) {
  gcu_audio_init(&song, gcu_board_audio_read, NULL, pcm_len,
                 GCU_DEFAULTS.volume_default);
  req_q = xQueueCreate(8, sizeof(int));
  ev_q = xQueueCreate(8, sizeof(int));
  xTaskCreate(audio_task, "audio", 4096, NULL, 5, NULL);
}

void gcu_board_audio_request(int req) {
  if (req_q) {
    xQueueSend(req_q, &req, 0);
  }
}

int gcu_board_audio_poll_event(void) {
  int ev;
  if (ev_q && xQueueReceive(ev_q, &ev, 0) == pdTRUE) {
    return ev;
  }
  return 0;
}

void gcu_board_audio_set_volume(int volume) {
  song.volume = volume; /* task reads per chunk; benign int write */
}

int gcu_board_audio_read(void *user, unsigned offset, unsigned char *dst,
                         int n) {
  (void)user;
  if (!audio_part ||
      esp_partition_read(audio_part, AUDIO_HDR_LEN + offset, dst,
                         (size_t)n) != ESP_OK) {
    return -1;
  }
  return n;
}

unsigned gcu_board_audio_probe(void) {
  uint8_t hdr[AUDIO_HDR_LEN];
  audio_part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, 0x40, "audio");
  audio_pcm_len = 0;
  if (!audio_part) {
    printf("audio=missing-partition\n");
    return 0;
  }
  if (esp_partition_read(audio_part, 0, hdr, sizeof hdr) != ESP_OK ||
      memcmp(hdr, AUDIO_MAGIC, 4) != 0) {
    printf("audio=missing\n"); /* clear link status; UI must stay usable */
    return 0;
  }
  unsigned rate, len;
  memcpy(&rate, hdr + 4, 4);
  memcpy(&len, hdr + 8, 4);
  if (len == 0 || len > audio_part->size - AUDIO_HDR_LEN) {
    printf("audio=missing\n");
    return 0;
  }
  audio_pcm_len = len;
  printf("audio=ok bytes=%u rate=%u\n", len, rate);
  return len;
}
