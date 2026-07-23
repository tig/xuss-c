#ifndef GCU_AUDIO_BOARD_H
#define GCU_AUDIO_BOARD_H

/* Probe the 'audio' flash partition for the full-track blob.
 * Prints a clear link status line; returns PCM byte length (0 = missing:
 * playback must be refused, UI stays usable — spec.md §4.4). */
unsigned gcu_board_audio_probe(void);

/* gcu_audio_read_fn over the audio partition (offsets past the header). */
int gcu_board_audio_read(void *user, unsigned offset, unsigned char *dst,
                         int n);

/* Playback task. Requests take gcu_audio_req_t values
 * (START/PAUSE/RESUME) or GCU_BOARD_AUDIO_RIFF (boot greeting, plays
 * asynchronously so the link/UI stay live); any other value parks
 * playback (repl/reboot). Events: 0 none, 1 natural song end, 2 error. */
#define GCU_BOARD_AUDIO_RIFF 101
void gcu_board_audio_task_start(unsigned pcm_len);
void gcu_board_audio_request(int req);
int gcu_board_audio_poll_event(void);
void gcu_board_audio_set_volume(int volume);

#endif
