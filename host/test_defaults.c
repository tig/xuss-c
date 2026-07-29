/* Product-path: drive domain with shipped GCU_DEFAULTS unmodified. */
#include "gcu/defaults.h"
#include "gcu/domain.h"
#include "gcu/hal.h"
#include "gcu/version.h"

#include <stdio.h>
#include <string.h>

static int led;
static int64_t fake_now;
static int paints;

static void set_led(gcu_hal_t *self, int on) {
  (void)self;
  led = on;
}

static void delay_ms(gcu_hal_t *self, int ms) {
  (void)self;
  (void)ms;
}

static int64_t now_ms(gcu_hal_t *self) {
  (void)self;
  return fake_now;
}

static void fill_rect(gcu_hal_t *self, int x, int y, int w, int h,
                      uint16_t rgb565) {
  (void)self;
  (void)x;
  (void)y;
  (void)w;
  (void)h;
  (void)rgb565;
  paints++;
}

static int btn0(gcu_hal_t *self) {
  (void)self;
  return 0;
}

int main(void) {
  gcu_hal_t hal = {
      .set_led = set_led,
      .delay_ms = delay_ms,
      .now_ms = now_ms,
      .fill_rect = fill_rect,
      .btn_a = btn0,
      .btn_b = btn0,
      .btn_c = btn0,
  };
  gcu_state_t st;
  char id[64];

  if (GCU_DEFAULTS.tick_sleep_ms != GCU_TICK_SLEEP_MS) {
    fprintf(stderr, "defaults table mismatch\n");
    return 1;
  }
  if (GCU_DEFAULTS.wink_period_ms != GCU_WINK_PERIOD_MS) {
    fprintf(stderr, "wink default missing from table\n");
    return 1;
  }

  gcu_init(&st, &hal);
  if (gcu_tick_sleep_ms(&st) != GCU_DEFAULTS.tick_sleep_ms) {
    fprintf(stderr, "init did not take shipped defaults\n");
    return 1;
  }

  gcu_tick(&st);
  if (st.tick_count != 1) {
    fprintf(stderr, "tick failed\n");
    return 1;
  }
  if (paints < 1) {
    fprintf(stderr, "expected paint on first ticks\n");
    return 1;
  }

  gcu_identity_line(id, (int)sizeof id);
  if (strstr(id, "fw_name=XUSSC") == NULL ||
      strstr(id, "fw_version=") == NULL) {
    fprintf(stderr, "identity line bad: %s\n", id);
    return 1;
  }

  /* Theme cycle pure logic */
  gcu_theme_t t0 = st.theme;
  st.prev_a = 0;
  /* Simulate press via direct cycle through colors API */
  gcu_theme_colors_t blue = gcu_theme_colors(GCU_THEME_BLUE);
  gcu_theme_colors_t black = gcu_theme_colors(GCU_THEME_BLACK);
  if (blue.fg565 == black.fg565 && blue.bg565 == black.bg565) {
    fprintf(stderr, "theme colors collapsed\n");
    return 1;
  }
  (void)t0;

  /* Wink after period */
  fake_now = GCU_DEFAULTS.wink_period_ms + 1;
  gcu_tick(&st);
  if (!st.wink_closed) {
    fprintf(stderr, "expected wink closed after period\n");
    return 1;
  }

  printf("OK defaults+identity+themes+wink\n");
  return 0;
}
