#ifndef GCU_HAL_LEDS_H
#define GCU_HAL_LEDS_H

#include "gcu/hal.h"

/* M5GO side LED bars: 10 SK6812 (GRB) on GPIO15, driven over RMT.
 * gcu_leds_init() sets up the channel + encoder; gcu_leds_set() paints all
 * ten LEDs one color (0,0,0 = off, used by the black theme). */
void gcu_leds_init(void);
void gcu_leds_set(gcu_hal_t *self, int r, int g, int b);

#endif
