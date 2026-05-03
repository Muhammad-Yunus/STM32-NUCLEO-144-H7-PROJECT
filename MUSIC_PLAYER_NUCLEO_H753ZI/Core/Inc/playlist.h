#ifndef __PLAYLIST_H
#define __PLAYLIST_H

#include <stdint.h>

uint8_t Playlist_Init(void);
uint8_t Playlist_PlayNext(void);
uint8_t Playlist_PlaySpecific(const char *name);
uint8_t Playlist_PlayPrev(void);
uint8_t Playlist_PlayNextStrict(void);
uint8_t Playlist_PlayPrevStrict(void);
uint16_t Playlist_GetCount(void);
const char *Playlist_GetNameByIndex(uint16_t index);

#endif /* __PLAYLIST_H */
