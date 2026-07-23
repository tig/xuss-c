#include "gcu/audio.h"

void gcu_audio_init(gcu_audio_t *a, gcu_audio_read_fn read, void *user,
                    unsigned length, int volume) {
  a->read = read;
  a->user = user;
  a->length = length;
  a->offset = 0;
  a->volume = volume;
  a->state = GCU_AUDIO_STOPPED;
}

int gcu_audio_start(gcu_audio_t *a) {
  if (a->length == 0 || !a->read) {
    return 0;
  }
  a->offset = 0;
  a->state = GCU_AUDIO_PLAYING;
  return 1;
}

void gcu_audio_pause(gcu_audio_t *a) {
  if (a->state == GCU_AUDIO_PLAYING) {
    a->state = GCU_AUDIO_PAUSED;
  }
}

int gcu_audio_resume(gcu_audio_t *a) {
  if (a->state != GCU_AUDIO_PAUSED) {
    return 0;
  }
  a->state = GCU_AUDIO_PLAYING;
  return 1;
}

void gcu_audio_stop(gcu_audio_t *a) {
  a->state = GCU_AUDIO_STOPPED;
  a->offset = 0;
}

int gcu_audio_next_chunk(gcu_audio_t *a, unsigned char *dst, int max) {
  if (a->state != GCU_AUDIO_PLAYING || max <= 0) {
    return 0;
  }
  unsigned remain = a->length - a->offset;
  if (remain == 0) {
    gcu_audio_stop(a); /* natural end: idle, offset 0, no auto-repeat */
    return 0;
  }
  int want = (int)(remain < (unsigned)max ? remain : (unsigned)max);
  int got = a->read(a->user, a->offset, dst, want);
  if (got <= 0) {
    gcu_audio_stop(a); /* read error: fail to idle, UI stays usable */
    return -1;        /* distinct from natural end so callers can report */
  }
  /* Volume: scale around the 128 midpoint (8-bit unsigned). */
  int vol = a->volume;
  if (vol < 0) vol = 0;
  if (vol > 127) vol = 127;
  for (int i = 0; i < got; i++) {
    int centered = (int)dst[i] - 128;
    dst[i] = (unsigned char)(128 + centered * vol / 127);
  }
  a->offset += (unsigned)got;
  return got;
}
