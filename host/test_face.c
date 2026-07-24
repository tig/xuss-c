#include "gcu/defaults.h"
#include "gcu/face.h"

#include <stdio.h>

#define CHECK(c)                                                               \
  do {                                                                         \
    if (!(c)) {                                                                \
      fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c);              \
      return 1;                                                                \
    }                                                                          \
  } while (0)

int main(void) {
  int period = GCU_DEFAULTS.wink_period_ms; /* 10000 */
  int close = GCU_DEFAULTS.wink_close_ms;   /* 200 */

  /* Wink is closed only in the first close_ms of each period (time-based). */
  CHECK(gcu_wink_is_closed(0, period, close) == 1);
  CHECK(gcu_wink_is_closed(close - 1, period, close) == 1);
  CHECK(gcu_wink_is_closed(close, period, close) == 0);
  CHECK(gcu_wink_is_closed(5000, period, close) == 0);
  CHECK(gcu_wink_is_closed(period, period, close) == 1);      /* next period */
  CHECK(gcu_wink_is_closed(period + 50, period, close) == 1); /* still closed */
  CHECK(gcu_wink_is_closed(period + close + 1, period, close) == 0);
  CHECK(gcu_wink_is_closed(0, 0, close) == 0); /* guard */

  /* Banner advances with wall-clock time and wraps within span. */
  int span = 400;
  CHECK(gcu_banner_offset(0, 60, span) == 0);
  CHECK(gcu_banner_offset(1000, 60, span) == 60);   /* 60 px/s */
  CHECK(gcu_banner_offset(1000, 500, span) == 100); /* 500 % 400 = 100 wrap */
  CHECK(gcu_banner_offset(0, 60, 0) == 0);          /* guard */

  printf("OK face\n");
  return 0;
}
