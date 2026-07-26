#ifndef HAL_BOARD_H
#define HAL_BOARD_H

#include "gcu/domain.h"
#include "gcu/hal.h"

gcu_hal_t *gcu_make_board_hal(void);

/* Device bring-up beyond portable HAL. */
void board_init(void);
void board_apply_view(const gcu_view_t *view, const gcu_state_t *st);
void board_poll_buttons(gcu_state_t *st);
void board_service_serial(gcu_state_t *st);
void board_park_outputs(void);
void board_hard_reset(void);

#endif
