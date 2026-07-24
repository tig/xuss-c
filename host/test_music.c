#include "gcu/music.h"

#include <stdio.h>

#define CHECK(c)                                                               \
  do {                                                                         \
    if (!(c)) {                                                                \
      fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c);              \
      return 1;                                                                \
    }                                                                          \
  } while (0)

int main(void) {
  gcu_music_t m;
  gcu_music_init(&m);
  CHECK(m.phase == GCU_MUSIC_IDLE && m.offset == 0);

  /* idle -> playing from 0 */
  CHECK(gcu_music_press(&m, 0) == GCU_MUSIC_PLAYING);
  CHECK(m.offset == 0);

  /* advance while playing, then pause keeps the offset */
  gcu_music_advance(&m, 500);
  CHECK(m.offset == 500);
  CHECK(gcu_music_press(&m, 0) == GCU_MUSIC_PAUSED);
  CHECK(m.offset == 500);

  /* advance while paused is ignored */
  gcu_music_advance(&m, 100);
  CHECK(m.offset == 500);

  /* paused -> resume from offset (not restart) */
  CHECK(gcu_music_press(&m, 0) == GCU_MUSIC_PLAYING);
  CHECK(m.offset == 500);

  /* natural end -> idle, offset 0, no loop */
  gcu_music_end(&m);
  CHECK(m.phase == GCU_MUSIC_IDLE && m.offset == 0);

  /* mute blocks starting from idle, UI unaffected */
  CHECK(gcu_music_press(&m, 1) == GCU_MUSIC_IDLE);
  CHECK(m.phase == GCU_MUSIC_IDLE);

  /* but a paused track may still resume even if muted */
  gcu_music_init(&m);
  gcu_music_press(&m, 0);         /* playing */
  gcu_music_advance(&m, 20);      /* offset 20 */
  gcu_music_press(&m, 0);         /* paused */
  CHECK(gcu_music_press(&m, 1) == GCU_MUSIC_PLAYING);
  CHECK(m.offset == 20);

  printf("OK music\n");
  return 0;
}
