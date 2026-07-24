/* Product-path: drive domain with shipped GCU_DEFAULTS unmodified. */
#include "gcu/defaults.h"
#include "gcu/domain.h"
#include "gcu/hal.h"
#include "gcu/version.h"

#include <stdio.h>
#include <string.h>

static int led;

static void set_led(gcu_hal_t *self, int on) {
  (void)self;
  led = on;
}

static void delay_ms(gcu_hal_t *self, int ms) {
  (void)self;
  (void)ms;
}

int main(void) {
  gcu_hal_t hal = {.set_led = set_led, .delay_ms = delay_ms};
  gcu_state_t st;
  char id[64];

  /* Compiled use of shipped table (not a parallel literal table). */
  if (GCU_DEFAULTS.tick_sleep_ms != GCU_TICK_SLEEP_MS ||
      GCU_DEFAULTS.wink_period_ms != GCU_WINK_PERIOD_MS ||
      GCU_DEFAULTS.wink_close_ms != GCU_WINK_CLOSE_MS ||
      GCU_DEFAULTS.details_refresh_ms != GCU_DETAILS_REFRESH_MS ||
      GCU_DEFAULTS.sample_rate_hz != GCU_SAMPLE_RATE_HZ ||
      GCU_DEFAULTS.volume != GCU_VOLUME_DEFAULT ||
      GCU_DEFAULTS.mute != GCU_MUTE_DEFAULT) {
    fprintf(stderr, "defaults table mismatch\n");
    return 1;
  }
  if (GCU_DEFAULTS.banner_text == NULL ||
      strcmp(GCU_DEFAULTS.banner_text, GCU_BANNER_TEXT) != 0) {
    fprintf(stderr, "banner default mismatch\n");
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

  gcu_identity_line(id, (int)sizeof id);
  if (strstr(id, "fw_name=") == NULL || strstr(id, "fw_version=") == NULL) {
    fprintf(stderr, "identity line bad: %s\n", id);
    return 1;
  }

  printf("OK defaults+identity\n");
  return 0;
}
