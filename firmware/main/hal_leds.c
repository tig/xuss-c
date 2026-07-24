/* SK6812 side LED strip — device headers allowed here (hal_leds).
 * 10 LEDs on GPIO15, GRB order. Driven with the RMT TX driver + a bytes
 * encoder whose bit0/bit1 symbols encode the SK6812 line timing at a 10 MHz
 * resolution (0.1 us / tick):
 *   bit0 = 0.3us high / 0.9us low   bit1 = 0.6us high / 0.6us low
 * followed by a >80us reset low. */
#include "hal_leds.h"

#include "driver/rmt_tx.h"
#include "esp_log.h"

#include <string.h>

#define LED_GPIO 15
#define LED_COUNT 10
#define LED_RES_HZ 10000000 /* 0.1 us per tick */

static const char *TAG = "hal_leds";
static rmt_channel_handle_t s_chan;
static rmt_encoder_handle_t s_encoder;
static int s_ok = 0;

void gcu_leds_init(void) {
  rmt_tx_channel_config_t chan_cfg = {
      .clk_src = RMT_CLK_SRC_DEFAULT,
      .gpio_num = LED_GPIO,
      .mem_block_symbols = 64,
      .resolution_hz = LED_RES_HZ,
      .trans_queue_depth = 4,
  };
  if (rmt_new_tx_channel(&chan_cfg, &s_chan) != ESP_OK) {
    ESP_LOGW(TAG, "rmt channel failed");
    return;
  }

  rmt_bytes_encoder_config_t enc_cfg = {
      .bit0 = {.level0 = 1, .duration0 = 3, .level1 = 0, .duration1 = 9},
      .bit1 = {.level0 = 1, .duration0 = 6, .level1 = 0, .duration1 = 6},
      .flags.msb_first = 1,
  };
  if (rmt_new_bytes_encoder(&enc_cfg, &s_encoder) != ESP_OK) {
    ESP_LOGW(TAG, "rmt encoder failed");
    return;
  }
  if (rmt_enable(s_chan) != ESP_OK) {
    ESP_LOGW(TAG, "rmt enable failed");
    return;
  }
  s_ok = 1;
}

void gcu_leds_set(gcu_hal_t *self, int r, int g, int b) {
  (void)self;
  if (!s_ok) {
    return;
  }
  uint8_t grb[3 * LED_COUNT];
  for (int i = 0; i < LED_COUNT; i++) {
    grb[i * 3 + 0] = (uint8_t)g; /* SK6812 = GRB */
    grb[i * 3 + 1] = (uint8_t)r;
    grb[i * 3 + 2] = (uint8_t)b;
  }
  rmt_transmit_config_t tx = {.loop_count = 0};
  rmt_transmit(s_chan, s_encoder, grb, sizeof grb, &tx);
  rmt_tx_wait_all_done(s_chan, 100);
}
