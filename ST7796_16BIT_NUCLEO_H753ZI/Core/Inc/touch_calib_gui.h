/**
 ******************************************************************************
 * @file    touch_calib_gui.h
 * @brief   XPT2046 touch screen calibration UI (depends on LCD/GUI)
 ******************************************************************************
 */

#ifndef __TOUCH_CALIB_GUI_H
#define __TOUCH_CALIB_GUI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"
#include <stdint.h>

/* Calibration margin from screen edges */
#define CAL_MARGIN  20

/* Public API */
void TP_Draw_Touch_Point(uint16_t x, uint16_t y, uint16_t color);
void TP_Draw_Big_Point(uint16_t x, uint16_t y, uint16_t color);
void TP_Adjust(void);

#ifdef __cplusplus
}
#endif

#endif /* __TOUCH_CALIB_GUI_H */
