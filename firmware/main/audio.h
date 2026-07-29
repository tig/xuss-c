#ifndef XUSSC_AUDIO_H
#define XUSSC_AUDIO_H

#include <stdint.h>

int audio_init(void);
int audio_play_pcm(const uint8_t *data, int len, int sample_rate);
int audio_busy(void);
void audio_stop(void);

#endif
