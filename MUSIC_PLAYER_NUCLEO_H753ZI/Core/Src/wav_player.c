#include "wav_player.h"
#include "audio_config.h"
#include "audio_synth.h"
#include "ff.h"
#include <stdio.h>
#include "main.h"

#define WAV_RING_FRAMES 8192U
#define WAV_RING_MASK (WAV_RING_FRAMES - 1U)
#define WAV_IO_CHUNK_BYTES 1024U

static FIL g_wav_file;
static uint32_t g_wav_data_size = 0U;
static uint32_t g_wav_bytes_played = 0U;
static uint8_t g_wav_mono = 0U;
static uint32_t g_wav_sample_rate = 0U;
static char g_wav_current_name[64];
static uint8_t g_wav_dump = 1U;

static int16_t g_ring_l[WAV_RING_FRAMES];
static int16_t g_ring_r[WAV_RING_FRAMES];
static volatile uint32_t g_ring_wr = 0U;
static volatile uint32_t g_ring_rd = 0U;
static volatile uint32_t g_ring_count = 0U;
static volatile uint32_t g_wav_underruns = 0U;

static uint8_t g_io_buf[WAV_IO_CHUNK_BYTES];
static uint32_t g_wav_last_log_ms = 0U;
static uint32_t g_wav_read_errors = 0U;

static uint32_t WAV_ReadLE32(const uint8_t *p)
{
  return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t WAV_ReadLE16(const uint8_t *p)
{
  return (uint16_t)(((uint16_t)p[0]) | ((uint16_t)p[1] << 8));
}

static uint32_t ring_free(void)
{
  return WAV_RING_FRAMES - g_ring_count;
}

static void ring_push(int16_t l, int16_t r)
{
  if (g_ring_count < WAV_RING_FRAMES) {
    g_ring_l[g_ring_wr] = l;
    g_ring_r[g_ring_wr] = r;
    g_ring_wr = (g_ring_wr + 1U) & WAV_RING_MASK;
    g_ring_count++;
  }
}

static uint8_t ring_pop(int16_t *l, int16_t *r)
{
  if (g_ring_count == 0U) return 0U;
  *l = g_ring_l[g_ring_rd];
  *r = g_ring_r[g_ring_rd];
  g_ring_rd = (g_ring_rd + 1U) & WAV_RING_MASK;
  g_ring_count--;
  return 1U;
}

uint8_t WavPlayer_Open(const char *name)
{
  f_close(&g_wav_file);

  uint8_t header[4096];
  UINT br = 0U;
  FRESULT fr = f_open(&g_wav_file, name, FA_READ);
  if (fr != FR_OK) {
    printf("WAV open failed: %s (FR=%d)\r\n", name, (int)fr);
    return 0U;
  }

  fr = f_read(&g_wav_file, header, sizeof(header), &br);
  if ((fr != FR_OK) || (br < 44U)) {
    printf("WAV header read failed: %s\r\n", name);
    f_close(&g_wav_file);
    return 0U;
  }

  if ((header[0] != 'R') || (header[1] != 'I') || (header[2] != 'F') || (header[3] != 'F') ||
      (header[8] != 'W') || (header[9] != 'A') || (header[10] != 'V') || (header[11] != 'E')) {
    printf("WAV not RIFF/WAVE: %s\r\n", name);
    f_close(&g_wav_file);
    return 0U;
  }

  uint16_t channels = 0U;
  uint32_t sample_rate = 0U;
  uint16_t bits_per_sample = 0U;
  uint32_t data_size = 0U;
  uint32_t data_offset = 0U;
  uint8_t found_fmt = 0U;
  uint32_t pos = 12U;

  while (pos + 8U < (uint32_t)br) {
    uint32_t chunk_size = WAV_ReadLE32(&header[pos + 4U]);
    if (g_wav_dump) {
      printf("CHK %.4s @%lu size=%lu\r\n", &header[pos], pos, chunk_size);
    }
    if ((header[pos] == 'f') && (header[pos + 1U] == 'm') && (header[pos + 2U] == 't') && (header[pos + 3U] == ' ')) {
      if (chunk_size >= 16U) {
        uint16_t audio_fmt = WAV_ReadLE16(&header[pos + 8U]);
        channels = WAV_ReadLE16(&header[pos + 10U]);
        sample_rate = WAV_ReadLE32(&header[pos + 12U]);
        bits_per_sample = WAV_ReadLE16(&header[pos + 22U]);
        if (audio_fmt == 1U) found_fmt = 1U;
      }
    } else if ((header[pos] == 'd') && (header[pos + 1U] == 'a') && (header[pos + 2U] == 't') && (header[pos + 3U] == 'a')) {
      data_size = chunk_size;
      data_offset = pos + 8U;
      break;
    }
    pos += 8U + chunk_size;
    if (chunk_size % 2U) pos += 1U;
  }

  if (!found_fmt || data_size == 0U || sample_rate != 16000U || bits_per_sample != 16U || (channels != 1U && channels != 2U)) {
    printf("WAV spec mismatch: %s (sr=%lu, bps=%u, ch=%u)\r\n", name, sample_rate, bits_per_sample, channels);
    f_close(&g_wav_file);
    return 0U;
  }

  if (f_lseek(&g_wav_file, data_offset) != FR_OK) {
    printf("WAV seek failed: %s\r\n", name);
    f_close(&g_wav_file);
    return 0U;
  }

  g_wav_data_size = data_size;
  g_wav_bytes_played = 0U;
  g_wav_mono = (channels == 1U) ? 1U : 0U;
  g_wav_sample_rate = sample_rate;
  g_ring_wr = g_ring_rd = g_ring_count = 0U;
  g_wav_underruns = 0U;
  g_wav_read_errors = 0U;
  g_wav_last_log_ms = HAL_GetTick();

  uint32_t ni = 0U;
  while ((name[ni] != 0) && (ni < sizeof(g_wav_current_name) - 1U)) {
    g_wav_current_name[ni] = name[ni];
    ni++;
  }
  g_wav_current_name[ni] = 0;

  printf("Playing WAV: %s (sr=%lu, ch=%u, data_ofs=%lu)\r\n", name, sample_rate, channels, data_offset);
  g_wav_dump = 0U;
  return 1U;
}

void WavPlayer_Pump(void)
{
  if (g_wav_sample_rate != 16000U) return;
  if (g_wav_bytes_played >= g_wav_data_size) return;
  if (ring_free() < 512U) return;

  uint32_t bytes_per_frame = g_wav_mono ? 2U : 4U;
  uint32_t left = g_wav_data_size - g_wav_bytes_played;
  uint32_t max_bytes = ring_free() * bytes_per_frame;
  uint32_t req = WAV_IO_CHUNK_BYTES;
  if (req > left) req = left;
  if (req > max_bytes) req = max_bytes;
  req -= (req % bytes_per_frame);
  if (req == 0U) return;

  UINT br = 0U;
  FRESULT fr = f_read(&g_wav_file, g_io_buf, req, &br);
  if (fr != FR_OK) {
    g_wav_read_errors++;
    return;
  }

  g_wav_bytes_played += br;
  uint32_t frames = br / bytes_per_frame;
  for (uint32_t i = 0; i < frames; i++) {
    int16_t l, r;
    if (g_wav_mono) {
      l = (int16_t)((uint16_t)g_io_buf[i * 2U] | ((uint16_t)g_io_buf[i * 2U + 1U] << 8));
      r = l;
    } else {
      uint32_t j = i * 4U;
      l = (int16_t)((uint16_t)g_io_buf[j] | ((uint16_t)g_io_buf[j + 1U] << 8));
      r = (int16_t)((uint16_t)g_io_buf[j + 2U] | ((uint16_t)g_io_buf[j + 3U] << 8));
    }
    ring_push(l, r);
  }

  uint32_t now = HAL_GetTick();
  if ((now - g_wav_last_log_ms) >= 1000U) {
    printf("WAVSTAT ring=%lu played=%lu/%lu underrun=%lu err=%lu\r\n",
           g_ring_count, g_wav_bytes_played, g_wav_data_size, g_wav_underruns, g_wav_read_errors);
    g_wav_last_log_ms = now;
  }
}

void WavPlayer_FillFrames(int16_t *buffer, uint16_t start_frame, uint16_t frame_count, float volume_gain)
{
  for (uint16_t i = 0; i < frame_count; i++) {
    uint16_t frame = (uint16_t)(start_frame + i);
    int16_t l = 0;
    int16_t r = 0;
    if (!ring_pop(&l, &r)) {
      g_wav_underruns++;
    }
    float gain = WAV_AMPLITUDE * volume_gain;
    buffer[frame * 2U] = AudioSynth_Sat16((int32_t)((float)l * gain));
    buffer[frame * 2U + 1U] = AudioSynth_Sat16((int32_t)((float)r * gain));
  }
}

uint8_t WavPlayer_IsFinished(void)
{
  return (g_wav_bytes_played >= g_wav_data_size && g_ring_count == 0U) ? 1U : 0U;
}

uint32_t WavPlayer_GetSampleRate(void)
{
  return g_wav_sample_rate;
}

uint32_t WavPlayer_GetElapsedSeconds(void)
{
  uint32_t bytes_per_frame = g_wav_mono ? 2U : 4U;
  if (g_wav_sample_rate == 0U || bytes_per_frame == 0U) {
    return 0U;
  }
  uint32_t frames = g_wav_bytes_played / bytes_per_frame;
  return frames / g_wav_sample_rate;
}

const char* WavPlayer_GetCurrentName(void)
{
  return g_wav_current_name;
}
