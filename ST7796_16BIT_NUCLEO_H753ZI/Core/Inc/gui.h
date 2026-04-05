/**
 ******************************************************************************
 * @file    gui.h
 * @brief   Basic 2-D GUI primitives adapted from Adafruit GFX style.
 *          Provides simple drawing, text rendering and minimal animation helpers.
 ******************************************************************************
 */

#ifndef __GUI_H
#define __GUI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lcd.h"
#include <stdint.h>

/* ── Font data (8×16) ─────────────────────────────────────────────────────── */
extern const uint8_t asc2_1608[95][16];

/* ── Global GUI state (similar to Adafruit GFX) ───────────────────────────── */
extern uint16_t GUI_TEXTCOLOR;   /* foreground color */
extern uint16_t GUI_BGCOLOR;     /* background color */
extern uint8_t  GUI_TEXTSIZE;    /* text scale factor (1 = original 8×16) */
extern uint16_t GUI_CURSOR_X;    /* text cursor X */
extern uint16_t GUI_CURSOR_Y;    /* text cursor Y */

/* ── Core pixel API ──────────────────────────────────────────────────────── */
void GUI_DrawPixel(uint16_t x, uint16_t y, uint16_t color);

/* ── Drawing primitives (Adafruit GFX style) ─────────────────────────────── */
void GUI_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void GUI_DrawFastVLine(uint16_t x, uint16_t y, uint16_t h, uint16_t color);
void GUI_DrawFastHLine(uint16_t x, uint16_t y, uint16_t w, uint16_t color);
void GUI_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void GUI_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void GUI_DrawCircle(uint16_t x0, uint16_t y0, uint8_t r, uint16_t color);
void GUI_FillCircle(uint16_t x0, uint16_t y0, uint8_t r, uint16_t color);
void GUI_DrawRoundRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t r, uint16_t color);
void GUI_FillRoundRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t r, uint16_t color);
void GUI_DrawTriangle(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void GUI_FillTriangle(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);

/* ── Text handling (Adafruit GFX style) ──────────────────────────────────── */
void GUI_SetCursor(uint16_t x, uint16_t y);
void GUI_SetTextColor(uint16_t fg, uint16_t bg);
void GUI_SetTextSize(uint8_t size);
void GUI_WriteChar(uint8_t c);
void GUI_Print(const char *str);
void GUI_Println(const char *str);
void GUI_PrintNum(int32_t num);
void GUI_PrintFloat(float num, uint8_t precision);

/* ── Compatibility wrappers (original API) ──────────────────────────────── */
void GUI_Fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t color);
void GUI_DrawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void GUI_ShowChar(uint16_t x, uint16_t y, uint8_t num, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode);
void GUI_ShowString(uint16_t x, uint16_t y, const uint8_t *p, uint16_t color);
void GUI_ShowNum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint16_t color);
void GUI_ShowIntNum(uint16_t x, uint16_t y, int32_t num, uint8_t len, uint16_t color);
void GUI_ShowFloatNum(uint16_t x, uint16_t y, float num, uint8_t precision, uint16_t color);

#ifdef __cplusplus
}
#endif

#endif /* __GUI_H */