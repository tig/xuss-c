/* Debounce: one edge per press, bounce suppression, held levels. */
#include "gcu/defaults.h"
#include "gcu/input.h"

#include <stdio.h>
#include <string.h>

static int fails;

#define CHECK(cond)                                             \
  do {                                                          \
    if (!(cond)) {                                              \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      fails++;                                                  \
    }                                                           \
  } while (0)

int main(void) {
  gcu_input_t in;
  unsigned char raw[GCU_INPUT_N] = {0, 0, 0};
  unsigned t = 0;
  unsigned db = (unsigned)GCU_DEFAULTS.debounce_ms;

  gcu_input_init(&in, 0);

  /* Bounce shorter than debounce emits nothing. */
  raw[0] = 1;
  CHECK(gcu_input_update(&in, raw, t + 1) == 0);
  raw[0] = 0;
  CHECK(gcu_input_update(&in, raw, t + 5) == 0);
  CHECK(gcu_input_update(&in, raw, t + 5 + db) == 0);
  CHECK(!gcu_input_held(&in, 0));

  /* Clean press: exactly one edge, then hold stays quiet. */
  t = 100;
  raw[0] = 1;
  CHECK(gcu_input_update(&in, raw, t) == 0);
  CHECK(gcu_input_update(&in, raw, t + db) == 0x1);
  CHECK(gcu_input_held(&in, 0));
  CHECK(gcu_input_update(&in, raw, t + 10 * db) == 0); /* hold: no repeat */

  /* Release then press again: a second edge. */
  raw[0] = 0;
  gcu_input_update(&in, raw, t + 11 * db);
  CHECK(gcu_input_update(&in, raw, t + 12 * db) == 0);
  CHECK(!gcu_input_held(&in, 0));
  raw[0] = 1;
  gcu_input_update(&in, raw, t + 13 * db);
  CHECK(gcu_input_update(&in, raw, t + 14 * db) == 0x1);

  /* Independent buttons: B and C edge separately. */
  memset(raw, 0, sizeof raw);
  gcu_input_init(&in, 0);
  raw[1] = 1;
  raw[2] = 1;
  gcu_input_update(&in, raw, 10);
  CHECK(gcu_input_update(&in, raw, 10 + db) == 0x6);

  if (fails) {
    return 1;
  }
  printf("OK input\n");
  return 0;
}
