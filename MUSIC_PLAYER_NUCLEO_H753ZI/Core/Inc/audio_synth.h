#ifndef __AUDIO_SYNTH_H
#define __AUDIO_SYNTH_H

#include <stdint.h>

void AudioSynth_Init(void);
void AudioSynth_StartNote(float freq_hz);
void AudioSynth_StartMelody(void);
void AudioSynth_FillFrames(int16_t *buffer, uint16_t start_frame, uint16_t frame_count);
int16_t AudioSynth_Sat16(int32_t v);

#endif /* __AUDIO_SYNTH_H */
