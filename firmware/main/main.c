#include "gcu/defaults.h"
#include "gcu/domain.h"
#include "gcu/hal.h"
#include "gcu/version.h"
#include "hal_board.h"

#include <stdio.h>

/*
 * Identity on the link (#78 / #79): boot-print alone is not enough for
 * silico inspect after the greeting scrolls past. The app must also answer
 * the host word "identity" (CR/LF framed) with fw_name=… fw_version=….
 *
 * Product face (spec Rev 0.3): boot riff, living face, side LEDs, buttons.
 * Audio may startle — announced by the agent before deploy.
 */
void app_main(void) {
  char id[64];
  gcu_state_t st;
  gcu_view_t view;

  /* HAL init must stay reachable from app_main (silico gate checks this). */
  gcu_hal_t *hal = gcu_make_board_hal();
  board_init();

  gcu_identity_line(id, (int)sizeof id);
  printf("%s\n", id);
  fflush(stdout);

  gcu_init(&st, hal);

  for (;;) {
    board_service_serial(&st);
    board_poll_buttons(&st);
    board_poll_domain_events(&st);
    gcu_tick(&st, &view);
    board_apply_view(&view, &st);
    /* Sample living UI into esprec ring/flash when a rec session is active. */
    board_esprec_rec_poll();
    if (hal && hal->delay_ms) {
      hal->delay_ms(hal, GCU_DEFAULTS.ui_tick_ms);
    }
  }
}
