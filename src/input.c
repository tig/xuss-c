#include "gcu/input.h"

#include "gcu/defaults.h"

void gcu_input_init(gcu_input_t *in, unsigned now_ms) {
  for (int i = 0; i < GCU_INPUT_N; i++) {
    in->stable[i] = 0;
    in->raw_prev[i] = 0;
    in->changed_ms[i] = now_ms;
  }
}

unsigned gcu_input_update(gcu_input_t *in,
                          const unsigned char pressed[GCU_INPUT_N],
                          unsigned now_ms) {
  unsigned edges = 0;
  for (int i = 0; i < GCU_INPUT_N; i++) {
    unsigned char raw = pressed[i] ? 1 : 0;
    if (raw != in->raw_prev[i]) {
      in->raw_prev[i] = raw;
      in->changed_ms[i] = now_ms;
      continue; /* not stable yet */
    }
    if (raw != in->stable[i] &&
        (int)(now_ms - in->changed_ms[i]) >= GCU_DEFAULTS.debounce_ms) {
      in->stable[i] = raw;
      if (raw) {
        edges |= 1u << i; /* one edge per press; hold emits nothing more */
      }
    }
  }
  return edges;
}

int gcu_input_held(const gcu_input_t *in, int btn) {
  return btn >= 0 && btn < GCU_INPUT_N ? in->stable[btn] : 0;
}
