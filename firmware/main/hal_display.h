#ifndef GCU_HAL_DISPLAY_H
#define GCU_HAL_DISPLAY_H

#include "gcu/gfx.h"

/* Bring up the M5GO ILI9342C IPS panel and return a gfx backend bound to it.
 * Returns NULL if the panel could not be initialized. */
gcu_gfx_t *gcu_make_display(void);

#endif
