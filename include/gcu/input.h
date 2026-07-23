#ifndef GCU_INPUT_H
#define GCU_INPUT_H

/* Front-button debounce — pure logic (spec.md §4.3: one edge per press;
 * hold does not fast-forward). Caller samples raw pressed levels. */

#define GCU_INPUT_N 3 /* A, B, C — indices match gcu_button_t */

typedef struct {
  unsigned char stable[GCU_INPUT_N];  /* debounced: 1 = pressed */
  unsigned char raw_prev[GCU_INPUT_N];
  unsigned changed_ms[GCU_INPUT_N];   /* when raw last changed */
} gcu_input_t;

void gcu_input_init(gcu_input_t *in, unsigned now_ms);

/* Feed raw samples (1 = pressed). Returns a bitmask of *new press edges*
 * (bit 0 = A, bit 1 = B, bit 2 = C) after debounce. */
unsigned gcu_input_update(gcu_input_t *in, const unsigned char pressed[GCU_INPUT_N],
                          unsigned now_ms);

/* Debounced held level (Details shows which buttons are held). */
int gcu_input_held(const gcu_input_t *in, int btn);

#endif
