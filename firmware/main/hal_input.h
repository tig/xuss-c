#ifndef GCU_HAL_INPUT_H
#define GCU_HAL_INPUT_H

#include "gcu/hal.h"

/* M5GO front buttons A/B/C. Configure the GPIOs, then read a pressed-mask
 * (bit0=A, bit1=B, bit2=C; 1 = pressed). */
void gcu_input_init(void);
int gcu_input_read(gcu_hal_t *self);

#endif
