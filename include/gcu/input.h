#ifndef GCU_INPUT_H
#define GCU_INPUT_H

/* Time-based button debounce (§4.3 "one edge per press"). Pure logic so host
 * tests cover the edge behavior without hardware. Bit 0 = A, 1 = B, 2 = C;
 * a set bit means "pressed". */

typedef struct {
  int stable_mask;   /* last committed pressed state */
  int cand_mask;     /* current candidate (raw) state */
  long cand_since[3]; /* when each candidate bit last changed */
} gcu_debounce_t;

void gcu_debounce_init(gcu_debounce_t *d, int mask);

/* Feed a raw sample. Returns the mask of buttons that just transitioned to
 * pressed (0->1) after being stable for `stable_ms`. */
int gcu_debounce_update(gcu_debounce_t *d, int raw_mask, long now_ms,
                        int stable_ms);

#endif
