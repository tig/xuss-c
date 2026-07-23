#ifndef GCU_AUDIO_H
#define GCU_AUDIO_H

/* Playback engine — pure logic over an abstract byte source (spec.md §4.4).
 * The device backend owns the DAC/DMA pump and the flash partition; host
 * tests drive this with synthetic buffers. Samples are unsigned 8-bit
 * mono PCM. */

typedef int (*gcu_audio_read_fn)(void *user, unsigned offset, unsigned char *dst,
                                 int n); /* bytes read, <0 on error */

typedef enum {
  GCU_AUDIO_STOPPED,
  GCU_AUDIO_PLAYING,
  GCU_AUDIO_PAUSED,
} gcu_audio_state_t;

typedef struct {
  gcu_audio_read_fn read;
  void *user;
  unsigned length; /* total PCM bytes; 0 = asset missing (refuse start) */
  unsigned offset; /* next byte to play; kept across pause */
  int volume;      /* 0..127 amplitude scale */
  int state;       /* gcu_audio_state_t */
} gcu_audio_t;

void gcu_audio_init(gcu_audio_t *a, gcu_audio_read_fn read, void *user,
                    unsigned length, int volume);

/* Start from byte 0. Returns 0 and stays STOPPED when the asset is
 * missing/empty (caller reports a clear link status; UI stays usable). */
int gcu_audio_start(gcu_audio_t *a);
void gcu_audio_pause(gcu_audio_t *a);  /* keep offset */
int gcu_audio_resume(gcu_audio_t *a);  /* from kept offset, not restart */
void gcu_audio_stop(gcu_audio_t *a);   /* offset back to 0 */

/* Fill dst with up to max volume-scaled samples while PLAYING.
 * Returns bytes produced; 0 means natural end (state -> STOPPED,
 * offset -> 0) or not playing. */
int gcu_audio_next_chunk(gcu_audio_t *a, unsigned char *dst, int max);

#endif
