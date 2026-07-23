#include "gcu/domain.h"

#include "app_board.h"
#include "audio_board.h"

#include <stdio.h>

void app_main(void) {
  char id[64];

  /* Identity first — never waits on audio (spec §4.1, §6.1). */
  gcu_identity_line(id, (int)sizeof id);
  printf("%s\n", id);
  fflush(stdout);

  gcu_board_boot_greeting(); /* riff: eased slice of First */
  unsigned song_len = gcu_board_audio_probe();

  gcu_board_app_run(song_len);
}
