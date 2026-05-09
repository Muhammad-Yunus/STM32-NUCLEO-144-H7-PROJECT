#ifndef __SIMPLE_GFX_H
#define __SIMPLE_GFX_H

#include <stdint.h>

/* -----------------------------------------------------------------------
 * Callback struct — app layer registers handlers for UI events.
 * simple_gfx dispatches through these pointers; NULL = no-op.
 * ----------------------------------------------------------------------- */
typedef struct {
  void (*on_play)(void);
  void (*on_pause)(void);
  void (*on_stop)(void);
  void (*on_next)(void);
  void (*on_prev)(void);
  void (*on_shuffle_toggle)(void);
  void (*on_track_select)(const char *name);
  void (*on_refresh_playlist)(void);
  void (*on_volume_change)(uint16_t volume);
  void (*on_brightness_change)(uint8_t level);
} SimpleGFX_Callbacks_t;

void SimpleGFX_SetCallbacks(const SimpleGFX_Callbacks_t *cb);

/* Lifecycle */
void SimpleGFX_Init(void);
void SimpleGFX_Update(void);

/* Playlist population */
void SimpleGFX_ClearPlaylist(void);
void SimpleGFX_AddTrackToList(const char *name);

/* State updates called by app layer */
void SimpleGFX_SetTrackName(const char *name);
void SimpleGFX_SetCurrentTrackByName(const char *name);
void SimpleGFX_SetPlaying(uint8_t playing);
void SimpleGFX_SetShuffle(uint8_t enabled);
void SimpleGFX_SetPlaybackTime(uint16_t mm, uint16_t ss);
void SimpleGFX_SetSpectrumLevels(const uint8_t *levels, uint8_t count);
void SimpleGFX_RenderSpectrumOnly(void);

/* UI touch dispatcher — called by player_app with calibrated coordinates.
 * Performs hit-testing against UI layout and mutates internal UI state.
 * Physical touch reading (ADC/calibration) lives in player_app. */
void SimpleGFX_ProcessTouch(uint16_t x, uint16_t y, uint8_t pressed);

#endif /* __SIMPLE_GFX_H */
