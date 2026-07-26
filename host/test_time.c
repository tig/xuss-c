#include "gcu/defaults.h"
#include "gcu/domain.h"

#include <stdio.h>

/* Host long is 64-bit; seed past 2^31 so millisecond math must use int64_t. */
static int64_t g_now;

static int64_t fake_now(gcu_hal_t *self) {
  (void)self;
  return g_now;
}

static gcu_hal_t g_hal = {.set_led = NULL, .delay_ms = NULL, .now_ms = fake_now};

int main(void) {
  gcu_state_t st;
  gcu_view_t view;

  /* Past signed-32-bit millisecond range (~24.8 days). */
  g_now = (int64_t)3000000000LL;
  gcu_init(&st, &g_hal);
  gcu_tick(&st, &view); /* consume boot repaint */

  /* Advance just under wink period — still open */
  g_now += GCU_DEFAULTS.wink_period_ms - 1;
  gcu_tick(&st, &view);
  if (view.wink_closed) {
    fprintf(stderr, "wink closed too early near 2^31 boundary\n");
    return 1;
  }

  /* Cross period — close */
  g_now += 2;
  gcu_tick(&st, &view);
  if (!view.wink_closed || !view.eye_repaint) {
    fprintf(stderr, "wink should close on period with eye_repaint\n");
    return 1;
  }

  /* Hold then reopen */
  g_now += GCU_DEFAULTS.wink_hold_ms + 1;
  gcu_tick(&st, &view);
  if (view.wink_closed) {
    fprintf(stderr, "wink should reopen after hold\n");
    return 1;
  }

  /* Banner moves right→left (offset decreases) on wall clock, not tick count */
  int off0 = view.banner_offset_px;
  g_now += GCU_DEFAULTS.banner_step_ms;
  gcu_tick(&st, &view);
  if (!view.banner_repaint) {
    fprintf(stderr, "banner should repaint on banner_step_ms\n");
    return 1;
  }
  if (view.banner_offset_px >= off0 &&
      !(off0 < 0 && view.banner_offset_px > 0)) {
    /* Allow wrap: large positive after going fully off the left edge. */
    if (view.banner_offset_px != GCU_DEFAULTS.display_w || off0 >= 0) {
      fprintf(stderr, "banner should move right→left (off %d → %d)\n", off0,
              view.banner_offset_px);
      return 1;
    }
  }

  printf("OK time64 wink+banner\n");
  return 0;
}
