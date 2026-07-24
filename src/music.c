#include "gcu/music.h"

void gcu_music_init(gcu_music_t *m) {
  if (!m) {
    return;
  }
  m->phase = GCU_MUSIC_IDLE;
  m->offset = 0;
}

gcu_music_phase_t gcu_music_press(gcu_music_t *m, int mute) {
  if (!m) {
    return GCU_MUSIC_IDLE;
  }
  switch (m->phase) {
  case GCU_MUSIC_IDLE:
    if (mute) {
      /* mute=1 blocks starting First (§4.4); stay idle, UI unaffected. */
      return m->phase;
    }
    m->offset = 0;
    m->phase = GCU_MUSIC_PLAYING;
    break;
  case GCU_MUSIC_PLAYING:
    m->phase = GCU_MUSIC_PAUSED; /* keep offset */
    break;
  case GCU_MUSIC_PAUSED:
    m->phase = GCU_MUSIC_PLAYING; /* resume from offset (not restart) */
    break;
  }
  return m->phase;
}

void gcu_music_end(gcu_music_t *m) {
  if (!m) {
    return;
  }
  m->phase = GCU_MUSIC_IDLE;
  m->offset = 0;
}

void gcu_music_advance(gcu_music_t *m, long amount) {
  if (!m || m->phase != GCU_MUSIC_PLAYING || amount < 0) {
    return;
  }
  m->offset += amount;
}

int gcu_music_is_playing(const gcu_music_t *m) {
  return m && m->phase == GCU_MUSIC_PLAYING;
}
