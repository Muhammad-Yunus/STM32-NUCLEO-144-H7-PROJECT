#ifndef __WAV_PLAYER_H
#define __WAV_PLAYER_H

#include <stdint.h>

uint8_t WavPlayer_Open(const char *name);
void WavPlayer_Pump(void);
void WavPlayer_FillFrames(int16_t *buffer, uint16_t start_frame, uint16_t frame_count, float volume_gain);
uint8_t WavPlayer_IsFinished(void);
uint32_t WavPlayer_GetSampleRate(void);
uint32_t WavPlayer_GetElapsedSeconds(void);
const char* WavPlayer_GetCurrentName(void);

#endif /* __WAV_PLAYER_H */
