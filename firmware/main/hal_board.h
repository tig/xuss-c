#ifndef GCU_HAL_BOARD_H
#define GCU_HAL_BOARD_H

#include "gcu/hal.h"

gcu_hal_t *gcu_make_board_hal(void);

/* Paint the side strip in the theme color (black = off). */
void gcu_board_led_theme(int theme);

#endif
