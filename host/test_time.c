/* Time contract: wall-clock wink uses int64_t milliseconds (#87).
 *
 * On ESP32 (Xtensa ILP32) `long` is 32 bits: millisecond math in `long`
 * overflows in <10 hours and wraps at ~24.8 days. Host `long` is 64-bit, so
 * this test seeds past 2^31 to catch the trap.
 */
#include "gcu/defaults.h"
#include "gcu/domain.h"
#include "gcu/hal.h"

#include <stdint.h>
#include <stdio.h>

static int64_t fake_now;

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
}

static int btn0(gcu_hal_t *self) {
  (void)self;
  return 0;
}

int main(void) {
  gcu_hal_t hal = {
      .delay_ms = delay_ms,
      .now_ms = now_ms,
      .fill_rect = fill_rect,
      .btn_a = btn0,
      .btn_b = btn0,
      .btn_c = btn0,
  };
  gcu_state_t st;
  const int period = GCU_DEFAULTS.wink_period_ms;

  fake_now = (INT64_C(1) << 31) + 12345;
  gcu_init(&st, &hal);
  st.boot_done = 1;

  gcu_tick(&st);
  if (st.wink_closed) {
    fprintf(stderr, "wink closed with no elapsed time\n");
    return 1;
  }

  fake_now += period;
  gcu_tick(&st);
  if (!st.wink_closed) {
    fprintf(stderr, "expected wink after %d ms\n", period);
    return 1;
  }

  /* Hold ends: eye reopens. */
  fake_now += GCU_DEFAULTS.wink_hold_ms + 1;
  gcu_tick(&st);
  if (st.wink_closed) {
    fprintf(stderr, "wink should reopen after hold\n");
    return 1;
  }

  /* Large jump past another 2^31 ms still advances wink. */
  fake_now += (INT64_C(1) << 31) + period;
  gcu_tick(&st);
  if (!st.wink_closed) {
    fprintf(stderr, "64-bit delta mishandled after big clock jump\n");
    return 1;
  }

  printf("OK time64\n");
  return 0;
}
