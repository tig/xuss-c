#include "gcu/input.h"

#include <stdio.h>

#define CHECK(c)                                                               \
  do {                                                                         \
    if (!(c)) {                                                                \
      fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c);              \
      return 1;                                                                \
    }                                                                          \
  } while (0)

int main(void) {
  gcu_debounce_t d;
  gcu_debounce_init(&d, 0);
  int db = 20;

  /* A pressed but not yet stable: no edge. */
  CHECK(gcu_debounce_update(&d, 0x1, 0, db) == 0);
  CHECK(gcu_debounce_update(&d, 0x1, 10, db) == 0);
  /* Stable long enough -> one press edge for A. */
  CHECK(gcu_debounce_update(&d, 0x1, 25, db) == 0x1);
  /* Holding does not re-fire. */
  CHECK(gcu_debounce_update(&d, 0x1, 100, db) == 0);
  CHECK(gcu_debounce_update(&d, 0x1, 200, db) == 0);

  /* Release (stable) then press again -> a new edge. */
  gcu_debounce_update(&d, 0x0, 210, db);
  gcu_debounce_update(&d, 0x0, 240, db); /* settle release */
  CHECK(gcu_debounce_update(&d, 0x1, 250, db) == 0);
  CHECK(gcu_debounce_update(&d, 0x1, 275, db) == 0x1);

  /* Bounce: rapid toggles never settle -> no edge. */
  gcu_debounce_init(&d, 0);
  CHECK(gcu_debounce_update(&d, 0x2, 0, db) == 0);
  CHECK(gcu_debounce_update(&d, 0x0, 5, db) == 0);
  CHECK(gcu_debounce_update(&d, 0x2, 10, db) == 0);
  CHECK(gcu_debounce_update(&d, 0x0, 15, db) == 0);

  /* Two buttons settle independently in one call window. */
  gcu_debounce_init(&d, 0);
  gcu_debounce_update(&d, 0x5, 0, db); /* A + C raw */
  CHECK(gcu_debounce_update(&d, 0x5, 30, db) == 0x5);

  printf("OK input\n");
  return 0;
}
