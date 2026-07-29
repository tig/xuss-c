#ifndef XUSSC_AUDIO_H
#define XUSSC_AUDIO_H

#include <stdint.h>

int audio_init(void);
/* Play RAM buffer (boot riff). */
int audio_play_pcm(const uint8_t *data, int len, int sample_rate);
/* Stream u8 mono PCM from filesystem path; start_offset for resume. */
int audio_play_file(const char *path, int sample_rate, int start_offset);
int audio_busy(void);
/* Byte offset into current file/stream (for pause/resume). */
int audio_position(void);
void audio_stop(void);

#endif
