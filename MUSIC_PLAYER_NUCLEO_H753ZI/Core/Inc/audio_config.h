#ifndef __AUDIO_CONFIG_H
#define __AUDIO_CONFIG_H

#include <stdint.h>

/* DMA buffer size in stereo frames */
#define AUDIO_FRAMES   512U

/* WAV output gain */
#define AUDIO_AMPLITUDE 0.01f
#define AUDIO_GAIN      5.0f
#define WAV_AMPLITUDE   (AUDIO_AMPLITUDE * AUDIO_GAIN)

/* Playback Mode: WAV-only */
#define AUDIO_OUTPUT_MODE_WAV_ONLY 1U

#endif /* __AUDIO_CONFIG_H */
