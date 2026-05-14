#include "player_app.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ff.h"
#include "sd_spi.h"
#include "audio_config.h"
#include "wav_player.h"
#include "playlist.h"
#include "simple_gfx.h"
#include "SEGGER_RTT.h"

#define APP_RTT_LOG(...) printf(__VA_ARGS__)

#define LCD_WIDTH 320
#define LCD_HEIGHT 240

static FATFS *s_fs = NULL;
static SAI_HandleTypeDef *s_hsai = NULL;
static SPI_HandleTypeDef *s_hspi = NULL;

static int16_t s_audio_buffer[AUDIO_FRAMES * 2U];
static uint8_t s_sd_test_done = 0;
static uint8_t s_audio_paused = 0U;
static volatile uint16_t s_volume_percent = 10U;
static uint8_t s_shuffle_enabled = 0U;
static uint8_t s_viz_levels[14] = {0};
static uint32_t s_viz_lfsr = 0xA5A5F00DU;
static volatile uint8_t s_bl_pct = 100U;

static void App_LogNowPlaying(const char *tag)
{
  const char *name = WavPlayer_GetCurrentName();
  APP_RTT_LOG("[%s] track=%s vol=%u paused=%u\r\n", tag, (name != NULL && name[0] != 0) ? name : "(none)", (unsigned)s_volume_percent, (unsigned)s_audio_paused);
}

static uint8_t Viz_NextRand(uint8_t min_v, uint8_t max_v)
{
  s_viz_lfsr ^= s_viz_lfsr << 13;
  s_viz_lfsr ^= s_viz_lfsr >> 17;
  s_viz_lfsr ^= s_viz_lfsr << 5;
  uint8_t span = (uint8_t)(max_v - min_v + 1U);
  return (uint8_t)(min_v + (s_viz_lfsr % span));
}

static void Viz_UpdateSyntheticLevels(void)
{
  for (uint8_t i = 0; i < 14U; i++) {
    s_viz_levels[i] = Viz_NextRand(2U, 6U);
  }
  SimpleGFX_SetSpectrumLevels(s_viz_levels, 14U);
}

static void Viz_UpdateFromAudioHalf(uint32_t base)
{
  (void)base;
}

static void Viz_UpdateFromAudioFull(uint32_t base)
{
  (void)base;
}

static void TouchCal_ApplyDefaults(touch_calibration_t *cal)
{
  cal->x_min = 215;
  cal->x_max = 2750;
  cal->y_min = 410;
  cal->y_max = 3815;
  cal->invert_x = false;
  cal->invert_y = false;
  cal->swap_xy = true;
}

static void RefreshPlaylistFromSD(void)
{
  if (!Playlist_Init()) {
    return;
  }

  SimpleGFX_ClearPlaylist();
  uint16_t count = Playlist_GetCount();
  for (uint16_t i = 0; i < count; i++) {
    SimpleGFX_AddTrackToList(Playlist_GetNameByIndex(i));
  }
}

static void AudioApp_FillFrames(uint16_t start_frame, uint16_t frame_count)
{
  if (s_audio_paused) {
    for (uint16_t i = 0; i < frame_count; i++) {
      uint16_t frame = (uint16_t)(start_frame + i);
      s_audio_buffer[frame * 2U] = 0;
      s_audio_buffer[frame * 2U + 1U] = 0;
    }
    return;
  }

  if (WavPlayer_IsFinished()) {
    if (!Playlist_PlayNext()) {
      /* End of playlist — output silence and stop playback state */
      SimpleGFX_SetPlaying(0U);
      for (uint16_t i = 0; i < frame_count; i++) {
        uint16_t frame = (uint16_t)(start_frame + i);
        s_audio_buffer[frame * 2U] = 0;
        s_audio_buffer[frame * 2U + 1U] = 0;
      }
      return;
    }
    SimpleGFX_SetTrackName(WavPlayer_GetCurrentName());
    SimpleGFX_SetCurrentTrackByName(WavPlayer_GetCurrentName());
    SimpleGFX_SetPlaying(1U);
    App_LogNowPlaying("AUTO_NEXT");
  }

  float volume_gain = ((float)s_volume_percent / 100.0f) * 1.8f;
  WavPlayer_FillFrames(s_audio_buffer, start_frame, frame_count, volume_gain);
}

static void PlayerApp_OnPlay(void)
{
  s_audio_paused = 0U;
  SimpleGFX_SetPlaying(1U);
  App_LogNowPlaying("PLAY");
}

static void PlayerApp_OnPause(void)
{
  s_audio_paused = 1U;
  SimpleGFX_SetPlaying(0U);
  App_LogNowPlaying("PAUSE");
}

static void PlayerApp_OnStop(void)
{
  s_audio_paused = 1U;
  SimpleGFX_SetPlaying(0U);
  App_LogNowPlaying("STOP");
}

static void PlayerApp_OnShuffleToggle(void)
{
  s_shuffle_enabled = s_shuffle_enabled ? 0U : 1U;
  SimpleGFX_SetShuffle(s_shuffle_enabled);
}

static void PlayerApp_OnNext(void)
{
  uint8_t ok = 0U;
  if (s_shuffle_enabled && Playlist_GetCount() > 0U) {
    uint16_t count = Playlist_GetCount();
    uint16_t start = (uint16_t)(rand() % count);
    for (uint16_t i = 0; i < count; i++) {
      uint16_t idx = (uint16_t)((start + i) % count);
      const char *name = Playlist_GetNameByIndex(idx);
      if (name != NULL && Playlist_PlaySpecific(name)) {
        ok = 1U;
        break;
      }
    }
  } else {
    ok = Playlist_PlayNextStrict();
  }

  if (ok) {
    s_audio_paused = 0U;
    SimpleGFX_SetTrackName(WavPlayer_GetCurrentName());
    SimpleGFX_SetCurrentTrackByName(WavPlayer_GetCurrentName());
    SimpleGFX_SetPlaying(1U);
    App_LogNowPlaying("NEXT");
  }
}

static void PlayerApp_OnPrev(void)
{
  uint8_t ok = 0U;
  if (s_shuffle_enabled && Playlist_GetCount() > 0U) {
    uint16_t count = Playlist_GetCount();
    uint16_t start = (uint16_t)(rand() % count);
    for (uint16_t i = 0; i < count; i++) {
      uint16_t idx = (uint16_t)((start + i) % count);
      const char *name = Playlist_GetNameByIndex(idx);
      if (name != NULL && Playlist_PlaySpecific(name)) {
        ok = 1U;
        break;
      }
    }
  } else {
    ok = Playlist_PlayPrevStrict();
  }

  if (ok) {
    s_audio_paused = 0U;
    SimpleGFX_SetTrackName(WavPlayer_GetCurrentName());
    SimpleGFX_SetCurrentTrackByName(WavPlayer_GetCurrentName());
    SimpleGFX_SetPlaying(1U);
    App_LogNowPlaying("PREV");
  }
}

static void PlayerApp_OnTrackSelect(const char *name)
{
  static uint32_t last_select_ms = 0U;
  if (name == NULL) {
    return;
  }

  uint32_t now = HAL_GetTick();
  if ((now - last_select_ms) < 250U) {
    return;
  }
  last_select_ms = now;

  const char *cur = WavPlayer_GetCurrentName();
  if (cur != NULL && cur[0] != 0 && strcmp(cur, name) == 0) {
    return;
  }

  if (Playlist_PlaySpecific(name)) {
    SimpleGFX_SetTrackName(WavPlayer_GetCurrentName());
    SimpleGFX_SetCurrentTrackByName(WavPlayer_GetCurrentName());
    SimpleGFX_SetPlaying(1U);
    App_LogNowPlaying("TRACK_SELECT");
  }
}

static void PlayerApp_OnRefreshPlaylist(void)
{
  if (!s_sd_test_done) {
    return;
  }
  RefreshPlaylistFromSD();
}

static void PlayerApp_OnBrightnessChange(uint8_t level)
{
  static const uint8_t lut[5] = {20U, 40U, 60U, 80U, 100U};
  if (level > 4U) level = 4U;
  s_bl_pct = lut[level];
}

static void PlayerApp_OnVolumeChange(uint16_t volume)
{
  if (volume > 100U) {
    volume = 100U;
  }
  s_volume_percent = volume;
  APP_RTT_LOG("[VOLUME] vol=%u\r\n", (unsigned)s_volume_percent);
}

void PlayerApp_Init(SAI_HandleTypeDef *hsai,
                    SPI_HandleTypeDef  *hspi,
                    FATFS              *fs,
                    touch_calibration_t *cal)
{
  s_hsai = hsai;
  s_hspi = hspi;
  s_fs = fs;

  TouchCal_ApplyDefaults(cal);

  /* Register UI event callbacks */
  static const SimpleGFX_Callbacks_t s_gfx_cb = {
    .on_play             = PlayerApp_OnPlay,
    .on_pause            = PlayerApp_OnPause,
    .on_stop             = PlayerApp_OnStop,
    .on_next             = PlayerApp_OnNext,
    .on_prev             = PlayerApp_OnPrev,
    .on_shuffle_toggle   = PlayerApp_OnShuffleToggle,
    .on_track_select     = PlayerApp_OnTrackSelect,
    .on_refresh_playlist = PlayerApp_OnRefreshPlaylist,
    .on_volume_change    = PlayerApp_OnVolumeChange,
    .on_brightness_change= PlayerApp_OnBrightnessChange,
  };
  SimpleGFX_SetCallbacks(&s_gfx_cb);

  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_14, GPIO_PIN_SET);
  HAL_Delay(20);

  s_hspi->Instance->CFG1 &= ~SPI_CFG1_MBR;
  s_hspi->Instance->CFG1 |= SPI_BAUDRATEPRESCALER_64;

  HAL_Delay(10);

  if (!SD_IO_Init()) {
    printf("SD Init: FAILED\r\n");
  }

  HAL_Delay(10);

  s_hspi->Instance->CFG1 &= ~SPI_CFG1_MBR;
  s_hspi->Instance->CFG1 |= SPI_BAUDRATEPRESCALER_16;

  {
    FRESULT fr = f_mount(s_fs, "", 1);
    if (fr == FR_OK) {
      s_sd_test_done = 1;
    } else {
      printf("Mount: FAILED (FR=%d)\r\n", (int)fr);
    }
  }

  HAL_Delay(10);

  if (Playlist_Init()) {
    RefreshPlaylistFromSD();

    if (Playlist_PlayNext()) {
      
      SimpleGFX_SetTrackName(WavPlayer_GetCurrentName());
      SimpleGFX_SetCurrentTrackByName(WavPlayer_GetCurrentName());
      SimpleGFX_SetPlaying(1U);
    } else {
      
      SimpleGFX_SetPlaying(0U);
    }
  } else {
    
    SimpleGFX_SetPlaying(0U);
  }

  AudioApp_FillFrames(0U, AUDIO_FRAMES);

  HAL_Delay(10);

  if (HAL_SAI_Transmit_DMA(s_hsai, (uint8_t *)s_audio_buffer, AUDIO_FRAMES * 2U) != HAL_OK) {
    printf("SAI DMA: FAILED\r\n");
    Error_Handler();
  }

  HAL_Delay(10);
}

void PlayerApp_Tick(touch_calibration_t *cal)
{
  static uint32_t last_gui_update = 0;
  static uint32_t last_viz_update = 0;
  uint32_t now = HAL_GetTick();

  uint16_t tx, ty;
  if (Touch4W_GetCalibratedXY(&tx, &ty, cal, LCD_WIDTH, LCD_HEIGHT)) {
      SimpleGFX_ProcessTouch(tx, ty, 1);
  } else {
      SimpleGFX_ProcessTouch(0, 0, 0);
  }

  if ((now - last_viz_update) >= 120U) {
    last_viz_update = now;
    if (!s_audio_paused) {
      Viz_UpdateSyntheticLevels();
    } else {
      for (uint8_t i = 0; i < 14U; i++) {
        s_viz_levels[i] = 0U;
      }
      SimpleGFX_SetSpectrumLevels(s_viz_levels, 14U);
    }
    SimpleGFX_RenderSpectrumOnly();
  }
  if ((now - last_gui_update) >= 180U) {
    last_gui_update = now;
    uint32_t elapsed = WavPlayer_GetElapsedSeconds();
    SimpleGFX_SetPlaybackTime((uint16_t)(elapsed / 60U), (uint16_t)(elapsed % 60U));
    SimpleGFX_Update();
  }

  WavPlayer_Pump();
}

void PlayerApp_OnAudioHalfTransfer(void)
{
  AudioApp_FillFrames(0U, AUDIO_FRAMES / 2U);
  Viz_UpdateFromAudioHalf(0U);
  SCB_CleanDCache_by_Addr((uint32_t*)&s_audio_buffer[0], AUDIO_FRAMES * sizeof(int16_t));
}

void PlayerApp_OnAudioFullTransfer(void)
{
  AudioApp_FillFrames(AUDIO_FRAMES / 2U, AUDIO_FRAMES / 2U);
  Viz_UpdateFromAudioFull(AUDIO_FRAMES);
  SCB_CleanDCache_by_Addr((uint32_t*)&s_audio_buffer[AUDIO_FRAMES], AUDIO_FRAMES * sizeof(int16_t));
}

uint8_t PlayerApp_GetBacklightPercent(void)
{
  return s_bl_pct;
}
