#include "gcu/input.h"

void gcu_debounce_init(gcu_debounce_t *d, int mask) {
  if (!d) {
    return;
  }
  d->stable_mask = mask;
  d->cand_mask = mask;
  for (int i = 0; i < 3; i++) {
    d->cand_since[i] = 0;
  }
}

int gcu_debounce_update(gcu_debounce_t *d, int raw_mask, long now_ms,
                        int stable_ms) {
  if (!d) {
    return 0;
  }
  int edges = 0;
  for (int i = 0; i < 3; i++) {
    int bit = 1 << i;
    int raw = raw_mask & bit;
    if ((d->cand_mask & bit) != raw) {
      /* raw differs from the candidate: restart this button's settle timer */
      d->cand_mask = (d->cand_mask & ~bit) | raw;
      d->cand_since[i] = now_ms;
      continue;
    }
    if (now_ms - d->cand_since[i] >= stable_ms &&
        (d->stable_mask & bit) != (d->cand_mask & bit)) {
      int was = d->stable_mask & bit;
      d->stable_mask = (d->stable_mask & ~bit) | (d->cand_mask & bit);
      if (!was && (d->stable_mask & bit)) {
        edges |= bit; /* press edge */
      }
    }
  }
  return edges;
}
