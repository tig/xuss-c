/* Device HAL backend — only TU allowlisted for device headers (with
 * audio_board). M5GO side RGB strip (SK6812 on GPIO15); pins come from
 * the shipped defaults table, not local literals. */
#include "hal_board.h"

#include "gcu/defaults.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"

static led_strip_handle_t strip;

static void set_led(gcu_hal_t *self, int on) {
  (void)self;
  if (!strip) {
    return;
  }
  if (on) {
    for (int i = 0; i < GCU_DEFAULTS.side_led_count; i++) {
      /* Default blue theme, modest brightness. */
      led_strip_set_pixel(strip, i, 0, 0, 64);
    }
    led_strip_refresh(strip);
  } else {
    led_strip_clear(strip);
  }
}

static void delay_ms(gcu_hal_t *self, int ms) {
  (void)self;
  vTaskDelay(pdMS_TO_TICKS(ms > 0 ? ms : 1));
}

static gcu_hal_t board_hal = {
    .set_led = set_led,
    .delay_ms = delay_ms,
};

gcu_hal_t *gcu_make_board_hal(void) {
  led_strip_config_t strip_cfg = {
      .strip_gpio_num = GCU_DEFAULTS.side_led_pin,
      .max_leds = (uint32_t)GCU_DEFAULTS.side_led_count,
      .led_model = LED_MODEL_SK6812,
      .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
  };
  led_strip_rmt_config_t rmt_cfg = {
      .resolution_hz = 10 * 1000 * 1000,
  };
  if (led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &strip) != ESP_OK) {
    strip = NULL; /* Face still runs; greeting and serial identity stay. */
  }
  return &board_hal;
}
