#ifndef __GUI_MANAGER_H
#define __GUI_MANAGER_H

#include <stdint.h>

void GuiManager_Init(void);
void GuiManager_Update(void);

/* Playlist population */
void GuiManager_ClearPlaylist(void);
void GuiManager_AddTrackToList(const char *name);

/* UI event callbacks to be implemented in main/playlist */
void GuiManager_OnPlay(void);
void GuiManager_OnPause(void);
void GuiManager_OnStop(void);
void GuiManager_OnNext(void);
void GuiManager_OnPrev(void);
void GuiManager_OnShuffleToggle(void);
void GuiManager_OnTrackSelect(const char *name);
void GuiManager_OnRefreshPlaylist(void);

/* State updates called by audio engine */
void GuiManager_SetTrackName(const char *name);
void GuiManager_SetCurrentTrackByName(const char *name);
void GuiManager_SetPlaying(uint8_t playing);
void GuiManager_SetShuffle(uint8_t enabled);
void GuiManager_SetPlaybackTime(uint16_t mm, uint16_t ss);
void GuiManager_SetSpectrumLevels(const uint8_t *levels, uint8_t count);
void GuiManager_RenderSpectrumOnly(void);
void GuiManager_HandleTouch(uint16_t x, uint16_t y, uint8_t pressed);

#endif /* __GUI_MANAGER_H */
