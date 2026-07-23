#ifndef GCU_AUDIO_BOARD_H
#define GCU_AUDIO_BOARD_H

/* Boot greeting: play the embedded riff from First on the speaker DAC.
 * Called after the identity line — identity never waits on audio. */
void gcu_board_boot_greeting(void);

/* Probe the 'audio' flash partition for the full-track blob.
 * Prints a clear link status line; returns PCM byte length (0 = missing:
 * playback must be refused, UI stays usable — spec.md §4.4). */
unsigned gcu_board_audio_probe(void);

/* gcu_audio_read_fn over the audio partition (offsets past the header). */
int gcu_board_audio_read(void *user, unsigned offset, unsigned char *dst,
                         int n);

#endif
