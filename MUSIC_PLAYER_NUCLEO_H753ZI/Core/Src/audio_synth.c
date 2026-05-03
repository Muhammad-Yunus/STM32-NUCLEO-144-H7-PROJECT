#include "audio_synth.h"
#include "audio_config.h"
#include <math.h>

typedef struct {
  float freq_hz;
  uint32_t duration_frames;
} audio_note_t;

static const audio_note_t g_melody[] = {
  {261.63f, 16000U}, {293.66f, 16000U}, {329.63f, 16000U},
  {349.23f, 16000U}, {392.00f, 16000U}, {440.00f, 16000U},
  {493.88f, 16000U}, {523.25f, 16000U}, {493.88f, 16000U},
  {440.00f, 16000U}, {392.00f, 16000U}, {349.23f, 16000U},
  {329.63f, 16000U}, {293.66f, 16000U}, {261.63f, 16000U},
};

static float audio_phase = 0.0f;
static float audio_phase_inc = 0.0f;
static uint32_t melody_note_index = 0U;
static uint32_t melody_note_frames_left = 0U;

int16_t AudioSynth_Sat16(int32_t v)
{
  if (v > 32767) return 32767;
  if (v < -32768) return -32768;
  return (int16_t)v;
}

void AudioSynth_Init(void)
{
  audio_phase = 0.0f;
  audio_phase_inc = 0.0f;
  melody_note_index = 0U;
  melody_note_frames_left = 0U;
}

void AudioSynth_StartNote(float freq_hz)
{
  audio_phase = 0.0f;
  audio_phase_inc = (2.0f * AUDIO_PI * freq_hz) / AUDIO_SAMPLE_RATE;
  melody_note_frames_left = 0xFFFFFFFFU; /* Perpetual */
}

void AudioSynth_StartMelody(void)
{
  melody_note_index = 0U;
  AudioSynth_StartNote(g_melody[0].freq_hz);
  melody_note_frames_left = g_melody[0].duration_frames;
}

void AudioSynth_FillFrames(int16_t *buffer, uint16_t start_frame, uint16_t frame_count)
{
  for (uint16_t i = 0; i < frame_count; i++) {
#if (AUDIO_OUTPUT_MODE != AUDIO_MODE_TEST)
    if (melody_note_frames_left == 0U) {
      melody_note_index++;
      if (melody_note_index >= (sizeof(g_melody) / sizeof(g_melody[0]))) {
        melody_note_index = 0U;
      }
      AudioSynth_StartNote(g_melody[melody_note_index].freq_hz);
      melody_note_frames_left = g_melody[melody_note_index].duration_frames;
    }
#endif

    float sample = AUDIO_AMPLITUDE * sinf(audio_phase);
    int16_t s = AudioSynth_Sat16((int32_t)(sample * 32767.0f));
    uint16_t frame = (uint16_t)(start_frame + i);
    buffer[frame * 2U] = s;
    buffer[frame * 2U + 1U] = s;

    audio_phase += audio_phase_inc;
    if (audio_phase >= (2.0f * AUDIO_PI)) {
      audio_phase -= (2.0f * AUDIO_PI);
    }

#if (AUDIO_OUTPUT_MODE != AUDIO_MODE_TEST)
    if (melody_note_frames_left != 0xFFFFFFFFU) {
      melody_note_frames_left--;
    }
#endif
  }
}
