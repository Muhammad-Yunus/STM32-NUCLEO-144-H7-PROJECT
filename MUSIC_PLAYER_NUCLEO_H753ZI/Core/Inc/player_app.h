#ifndef __PLAYER_APP_H
#define __PLAYER_APP_H

/* -----------------------------------------------------------------------
 * player_app — application orchestration layer
 *
 * Owns: audio state, playlist logic, UI callbacks, viz, touch cal,
 *       the main-loop tick, and SAI DMA half/full transfer handling.
 *
 * Depends on: simple_gfx.h, wav_player.h, playlist.h, audio_synth.h,
 *             audio_config.h, touch_4wire.h, ff.h, sd_spi.h
 * Does NOT depend on: lcd_spi_port.h, ili9341.h, main.c internals.
 * ----------------------------------------------------------------------- */

#include "stm32h7xx_hal.h"
#include "touch_4wire.h"
#include "ff.h"

/* Called once from main() after all peripherals and display are initialised */
void PlayerApp_Init(SAI_HandleTypeDef *hsai,
                    SPI_HandleTypeDef  *hspi,
                    FATFS              *fs,
                    touch_calibration_t *cal);

/* Called every iteration of the main while(1) loop */
void PlayerApp_Tick(touch_calibration_t *cal);

/* Forwarded from HAL SAI DMA callbacks in main.c */
void PlayerApp_OnAudioHalfTransfer(void);
void PlayerApp_OnAudioFullTransfer(void);

/* Backlight PWM percentage for TIM6 ISR in main.c */
uint8_t PlayerApp_GetBacklightPercent(void);

#endif /* __PLAYER_APP_H */
