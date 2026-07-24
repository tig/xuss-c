#ifndef GCU_MUSIC_H
#define GCU_MUSIC_H

/* Full-song play/pause/resume state machine (§4.4). No auto-loop. */

typedef enum {
  GCU_MUSIC_IDLE = 0,
  GCU_MUSIC_PLAYING,
  GCU_MUSIC_PAUSED
} gcu_music_phase_t;

typedef struct {
  gcu_music_phase_t phase;
  long offset; /* resume offset into the track (0 at start/end) */
} gcu_music_t;

void gcu_music_init(gcu_music_t *m);

/* Button B press.
 *   idle    -> playing from offset 0 (blocked to idle when mute != 0)
 *   playing -> paused, keep offset
 *   paused  -> playing, resume from offset
 * Returns the resulting phase. */
gcu_music_phase_t gcu_music_press(gcu_music_t *m, int mute);

/* Natural end of track: back to idle, offset 0, no repeat. */
void gcu_music_end(gcu_music_t *m);

/* Device reports it consumed `amount` of samples/bytes while playing. */
void gcu_music_advance(gcu_music_t *m, long amount);

int gcu_music_is_playing(const gcu_music_t *m);

#endif
