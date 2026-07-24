/* M5GO front buttons — device headers allowed here (hal_input).
 * Button A=GPIO39, B=GPIO38, C=GPIO37 (input-only pins, external pull-ups on
 * the board). Pressed reads LOW. */
#include "hal_input.h"

#include "driver/gpio.h"

#define PIN_BTN_A 39
#define PIN_BTN_B 38
#define PIN_BTN_C 37

void gcu_input_init(void) {
  gpio_config_t io = {
      .pin_bit_mask = (1ULL << PIN_BTN_A) | (1ULL << PIN_BTN_B) |
                      (1ULL << PIN_BTN_C),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE, /* pins 34-39 have no internal pulls */
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  gpio_config(&io);
}

int gcu_input_read(gcu_hal_t *self) {
  (void)self;
  int mask = 0;
  if (gpio_get_level(PIN_BTN_A) == 0) {
    mask |= 1 << 0;
  }
  if (gpio_get_level(PIN_BTN_B) == 0) {
    mask |= 1 << 1;
  }
  if (gpio_get_level(PIN_BTN_C) == 0) {
    mask |= 1 << 2;
  }
  return mask;
}
