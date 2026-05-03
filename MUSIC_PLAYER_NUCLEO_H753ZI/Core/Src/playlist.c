#include "playlist.h"
#include "wav_player.h"
#include "ff.h"
#include <string.h>

#define MAX_PLAYLIST_ENTRIES 32
#define MAX_TRACK_NAME_LEN 64

static DIR g_wav_dir;
static char s_track_list[MAX_PLAYLIST_ENTRIES][MAX_TRACK_NAME_LEN];
static uint16_t s_track_count = 0;
static int16_t s_current_index = -1;

static uint8_t WAV_HasExt(const char *name)
{
  uint32_t len = 0U;
  while (name[len] != 0) {
    len++;
  }
  if (len < 4U) return 0U;

  char c0 = name[len - 4U];
  char c1 = name[len - 3U];
  char c2 = name[len - 2U];
  char c3 = name[len - 1U];

  if (c0 != '.') return 0U;

  if ((c1 == 'w' || c1 == 'W') &&
      (c2 == 'a' || c2 == 'A') &&
      (c3 == 'v' || c3 == 'V')) {
    return 1U;
  }

  return 0U;
}

uint8_t Playlist_Init(void)
{
  s_track_count = 0;
  s_current_index = -1;
  DIR dir;
  FILINFO fno;
  if (f_opendir(&dir, "/") != FR_OK) return 0U;

  while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0] != 0) {
    const char *name = fno.fname;
    if (!(fno.fattrib & AM_DIR) && WAV_HasExt(name)) {
      if (s_track_count < MAX_PLAYLIST_ENTRIES) {
        strncpy(s_track_list[s_track_count], name, MAX_TRACK_NAME_LEN - 1U);
        s_track_list[s_track_count][MAX_TRACK_NAME_LEN - 1U] = 0;
        s_track_count++;
      }
    }
  }
  f_closedir(&dir);

  return (f_opendir(&g_wav_dir, "/") == FR_OK) ? 1U : 0U;
}

uint16_t Playlist_GetCount(void)
{
  return s_track_count;
}

const char *Playlist_GetNameByIndex(uint16_t index)
{
  if (index >= s_track_count) return NULL;
  return s_track_list[index];
}

uint8_t Playlist_PlaySpecific(const char *name)
{
  if (name == NULL || s_track_count == 0U) return 0U;
  for (uint16_t i = 0; i < s_track_count; i++) {
    if (strcmp(s_track_list[i], name) == 0) {
      if (WavPlayer_Open(name)) {
        s_current_index = (int16_t)i;
        return 1U;
      }
      return 0U;
    }
  }
  return 0U;
}

uint8_t Playlist_PlayNextStrict(void)
{
  if (s_track_count == 0U) return 0U;
  if (s_current_index < 0) {
    s_current_index = 0;
  } else {
    s_current_index++;
    if (s_current_index >= (int16_t)s_track_count) s_current_index = 0;
  }
  return WavPlayer_Open(s_track_list[s_current_index]);
}

uint8_t Playlist_PlayPrevStrict(void)
{
  if (s_track_count == 0U) return 0U;
  if (s_current_index < 0) {
    s_current_index = 0;
  } else if (s_current_index == 0) {
    s_current_index = (int16_t)(s_track_count - 1U);
  } else {
    s_current_index--;
  }
  return WavPlayer_Open(s_track_list[s_current_index]);
}

uint8_t Playlist_PlayNext(void)
{
  if (s_track_count == 0U) return 0U;
  int16_t idx = s_current_index;
  for (uint16_t tries = 0; tries < s_track_count; tries++) {
    if (idx < 0) {
      idx = 0;
    } else {
      idx++;
      if (idx >= (int16_t)s_track_count) idx = 0;
    }
    if (WavPlayer_Open(s_track_list[idx])) {
      s_current_index = idx;
      return 1U;
    }
  }
  return 0U;
}

uint8_t Playlist_PlayPrev(void)
{
  if (s_track_count == 0U) return 0U;
  int16_t idx = s_current_index;
  for (uint16_t tries = 0; tries < s_track_count; tries++) {
    if (idx < 0) {
      idx = 0;
    } else if (idx == 0) {
      idx = (int16_t)(s_track_count - 1U);
    } else {
      idx--;
    }
    if (WavPlayer_Open(s_track_list[idx])) {
      s_current_index = idx;
      return 1U;
    }
  }
  return 0U;
}
