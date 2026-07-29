/* Side SK6812 strip — allowlisted for led_strip / RMT. */
#include "leds.h"

#include "gcu/defaults.h"

#include "esp_log.h"
#include "led_strip.h"

static const char *TAG = "leds";
static led_strip_handle_t s_strip;
static int s_ready;

int leds_init(void) {
  if (s_ready) {
    return 0;
  }
  led_strip_config_t strip_cfg = {
      .strip_gpio_num = GCU_PIN_SIDE_LED,
      .max_leds = GCU_SIDE_LED_COUNT,
      .led_model = LED_MODEL_SK6812,
      .led_pixel_format = LED_PIXEL_FORMAT_GRB,
  };
  led_strip_rmt_config_t rmt_cfg = {
      .resolution_hz = 10 * 1000 * 1000,
      .flags.with_dma = false,
  };
  esp_err_t err = led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &s_strip);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "led_strip init failed: %s", esp_err_to_name(err));
    return -1;
  }
  led_strip_clear(s_strip);
  s_ready = 1;
  ESP_LOGI(TAG, "side strip ready gpio=%d count=%d", GCU_PIN_SIDE_LED,
           GCU_SIDE_LED_COUNT);
  return 0;
}

void leds_set_rgb(uint8_t r, uint8_t g, uint8_t b) {
  if (!s_ready || !s_strip) {
    return;
  }
  for (int i = 0; i < GCU_SIDE_LED_COUNT; i++) {
    led_strip_set_pixel(s_strip, i, r, g, b);
  }
  led_strip_refresh(s_strip);
}
