/* Audio engine: start/pause/resume offsets, natural end, volume, missing
 * asset (spec.md §4.4). */
#include "gcu/audio.h"

#include <stdio.h>

static int fails;

#define CHECK(cond)                                             \
  do {                                                          \
    if (!(cond)) {                                              \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      fails++;                                                  \
    }                                                           \
  } while (0)

/* Synthetic source: byte at offset i is (i & 0xff). */
static int src_read(void *user, unsigned offset, unsigned char *dst, int n) {
  (void)user;
  for (int i = 0; i < n; i++) {
    dst[i] = (unsigned char)((offset + (unsigned)i) & 0xffu);
  }
  return n;
}

static int err_read(void *user, unsigned offset, unsigned char *dst, int n) {
  (void)user;
  (void)offset;
  (void)dst;
  (void)n;
  return -1;
}

int main(void) {
  gcu_audio_t a;
  unsigned char buf[64];

  /* Missing asset refuses start. */
  gcu_audio_init(&a, src_read, NULL, 0, 127);
  CHECK(gcu_audio_start(&a) == 0);
  CHECK(a.state == GCU_AUDIO_STOPPED);

  /* Full-volume passthrough + sequential offsets. */
  gcu_audio_init(&a, src_read, NULL, 100, 127);
  CHECK(gcu_audio_start(&a) == 1);
  CHECK(gcu_audio_next_chunk(&a, buf, 64) == 64);
  CHECK(buf[0] == 0 && buf[63] == 63);
  CHECK(a.offset == 64);

  /* Pause keeps offset; resume continues (not restart). */
  gcu_audio_pause(&a);
  CHECK(a.state == GCU_AUDIO_PAUSED);
  CHECK(gcu_audio_next_chunk(&a, buf, 64) == 0); /* silent while paused */
  CHECK(a.offset == 64);
  CHECK(gcu_audio_resume(&a) == 1);
  CHECK(gcu_audio_next_chunk(&a, buf, 64) == 36); /* clamped at end */
  CHECK(buf[0] == 64);

  /* Natural end: STOPPED, offset 0, no auto-repeat. */
  CHECK(gcu_audio_next_chunk(&a, buf, 64) == 0);
  CHECK(a.state == GCU_AUDIO_STOPPED);
  CHECK(a.offset == 0);

  /* Restart after end plays from byte 0. */
  CHECK(gcu_audio_start(&a) == 1);
  CHECK(gcu_audio_next_chunk(&a, buf, 4) == 4);
  CHECK(buf[0] == 0);

  /* Volume scaling around the 128 midpoint. */
  gcu_audio_init(&a, src_read, NULL, 512, 0);
  gcu_audio_start(&a);
  a.offset = 200; /* byte value 200 -> centered +72 -> scaled 0 -> 128 */
  CHECK(gcu_audio_next_chunk(&a, buf, 1) == 1);
  CHECK(buf[0] == 128);
  a.volume = 64;
  CHECK(gcu_audio_next_chunk(&a, buf, 1) == 1); /* value 201 -> +73*64/127 */
  CHECK(buf[0] == (unsigned char)(128 + 73 * 64 / 127));

  /* Resume from nothing fails cleanly. */
  gcu_audio_init(&a, src_read, NULL, 10, 127);
  CHECK(gcu_audio_resume(&a) == 0);

  /* Read error is distinct from natural end and stops playback. */
  gcu_audio_init(&a, err_read, NULL, 100, 127);
  gcu_audio_start(&a);
  CHECK(gcu_audio_next_chunk(&a, buf, 16) == -1);
  CHECK(a.state == GCU_AUDIO_STOPPED);

  if (fails) {
    return 1;
  }
  printf("OK audio\n");
  return 0;
}
