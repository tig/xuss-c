#ifndef GCU_HAL_BOARD_H
#define GCU_HAL_BOARD_H

#include "gcu/hal.h"

gcu_hal_t *gcu_make_board_hal(void);

/* Boot greeting on the product speaker (short DAC tone). Called from
 * app_main after the identity line — identity never waits on audio. */
void gcu_board_boot_greeting(void);

#endif
