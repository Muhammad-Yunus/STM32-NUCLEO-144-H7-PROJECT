#ifndef __AUDIO_CONFIG_H
#define __AUDIO_CONFIG_H

#include <stdint.h>

/* Audio Engine Constants */
#define AUDIO_FRAMES            512U
#define AUDIO_SAMPLE_RATE       16000.0f
#define AUDIO_PI                3.14159265358979323846f

/* Gain Settings */
#define AUDIO_AMPLITUDE         0.01f
#define AUDIO_GAIN              5.0f
#define WAV_AMPLITUDE           (AUDIO_AMPLITUDE * AUDIO_GAIN)

/* Playback Modes */
#define AUDIO_MODE_WAV_OR_MELODY 0U
#define AUDIO_MODE_TEST          1U
#define AUDIO_OUTPUT_MODE        AUDIO_MODE_WAV_OR_MELODY
#define AUDIO_TEST_FREQ_HZ       1000.0f

#endif /* __AUDIO_CONFIG_H */
