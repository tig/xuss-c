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
static unsigned audio_rate;    /* from XCA1 header (validated) */

static int riff_read(void *user, unsigned offset, unsigned char *dst, int n) {
  (void)user;
  memcpy(dst, riff_start + offset, (size_t)n);
  return n;
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
  audio_rate = (unsigned)GCU_DEFAULTS.tone_sample_hz;
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
  if (rate < 8000 || rate > 48000) {
    printf("audio=bad-rate rate=%u\n", rate); /* refuse; do not play wrong */
    return 0;
  }
  audio_rate = rate; /* DAC clocks the song at the asset's own rate */
  audio_pcm_len = len;
  printf("audio=ok bytes=%u rate=%u\n", len, rate);
  return len;
}

/* --- Playback task (spec §5.2: audio never blocks UI/link).
 * Plays the boot riff (embedded) and the full song (partition); the
 * riff has priority when both are pending (boot only). --- */

static QueueHandle_t req_q;
static QueueHandle_t ev_q;
static gcu_audio_t song;
static gcu_audio_t riff;

/* DMA queue kept shallow so teardown/pause discard little audio:
 * 2 x 1024 bytes ≈ 93 ms at 22050 Hz. Drain must cover the full queue
 * before disable or endings hard-cut (spec §4.1/§4.4; review P1). */
#define DAC_DESC_NUM 2
#define DAC_BUF_SIZE 1024
#define DAC_DRAIN_MS 220

static dac_continuous_handle_t dac;
static int dac_on;
static unsigned dac_cur_rate;

static void dac_teardown(int drain) {
  if (!dac) {
    return;
  }
  if (dac_on) {
    if (drain) {
      vTaskDelay(pdMS_TO_TICKS(DAC_DRAIN_MS));
    }
    dac_continuous_disable(dac);
  }
  dac_continuous_del_channels(dac);
  dac = NULL;
  dac_on = 0;
}

static int dac_bringup(unsigned rate) {
  if (dac_on && dac_cur_rate == rate) {
    return 1;
  }
  dac_teardown(0);
  dac_continuous_config_t cfg = {
      .chan_mask = DAC_CHANNEL_MASK_CH0, /* GPIO25: shipped speaker pin */
      .desc_num = DAC_DESC_NUM,
      .buf_size = DAC_BUF_SIZE,
      .freq_hz = (uint32_t)rate,
      .offset = 0,
      .clk_src = DAC_DIGI_CLK_SRC_DEFAULT,
      .chan_mode = DAC_CHANNEL_MODE_SIMUL,
  };
  if (dac_continuous_new_channels(&cfg, &dac) != ESP_OK) {
    dac = NULL;
    return 0;
  }
  if (dac_continuous_enable(dac) != ESP_OK) {
    dac_continuous_del_channels(dac); /* review P2: don't leak the claim */
    dac = NULL;
    return 0;
  }
  dac_on = 1;
  dac_cur_rate = rate;
  return 1;
}

static void audio_task(void *arg) {
  (void)arg;
  static uint8_t chunk[DAC_BUF_SIZE];

  for (;;) {
    int req;
    int busy =
        song.state == GCU_AUDIO_PLAYING || riff.state == GCU_AUDIO_PLAYING;
    TickType_t wait = busy ? 0 : pdMS_TO_TICKS(50);
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
        case GCU_BOARD_AUDIO_RIFF:
          gcu_audio_start(&riff);
          break;
        default: /* park/stop (repl, reboot) */
          gcu_audio_stop(&song);
          gcu_audio_stop(&riff);
          break;
      }
      wait = 0;
    }

    if (riff.state == GCU_AUDIO_PLAYING) {
      if (!dac_bringup((unsigned)GCU_DEFAULTS.tone_sample_hz)) {
        gcu_audio_stop(&riff);
        continue;
      }
      int n = gcu_audio_next_chunk(&riff, chunk, (int)sizeof chunk);
      if (n > 0) {
        size_t written = 0;
        dac_continuous_write(dac, chunk, (size_t)n, &written, 1000);
      }
      /* riff end: engine resets itself; no UI event needed */
    } else if (song.state == GCU_AUDIO_PLAYING) {
      if (!dac_bringup(audio_rate)) {
        gcu_audio_stop(&song);
        int ev = 2;
        xQueueSend(ev_q, &ev, 0);
        continue;
      }
      int n = gcu_audio_next_chunk(&song, chunk, (int)sizeof chunk);
      if (n > 0) {
        size_t written = 0;
        dac_continuous_write(dac, chunk, (size_t)n, &written, 1000);
      } else {
        int ev = n < 0 ? 2 : 1; /* read error vs natural end (review P2) */
        xQueueSend(ev_q, &ev, 0);
      }
    }

    if (song.state != GCU_AUDIO_PLAYING && riff.state != GCU_AUDIO_PLAYING &&
        dac_on) {
      dac_teardown(1); /* full drain: no click, minimal discarded audio */
    }
  }
}

void gcu_board_audio_task_start(unsigned pcm_len) {
  gcu_audio_init(&song, gcu_board_audio_read, NULL, pcm_len,
                 GCU_DEFAULTS.volume_default);
  gcu_audio_init(&riff, riff_read, NULL, (unsigned)(riff_end - riff_start),
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
  riff.volume = volume;
}
