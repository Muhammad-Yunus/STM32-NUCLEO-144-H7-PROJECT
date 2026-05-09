#ifndef __SIMPLE_GFX_DRAW_H
#define __SIMPLE_GFX_DRAW_H

#include <stdint.h>

/* -----------------------------------------------------------------------
 * simple_gfx_draw — stateless drawing primitives for LCD
 *
 * Pure rendering helpers: rectangles, text, triangles, 7-segment digits.
 * No UI state, no callbacks — just pixel output via lcd_spi_port.
 * ----------------------------------------------------------------------- */

void GFX_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t c);
void GFX_DrawText5x7(uint16_t x, uint16_t y, const char *s, uint16_t c, uint16_t bg);
void GFX_DrawText5x7Ellipsis(uint16_t x, uint16_t y, const char *s, uint16_t c, uint16_t bg, uint16_t max_chars);
void GFX_DrawText5x7Scale3_2(uint16_t x, uint16_t y, const char *s, uint16_t c, uint16_t bg);
void GFX_DrawTriangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, uint16_t c);
void GFX_DrawDseg16Text(uint16_t x, uint16_t y, const char *s, uint16_t c, uint16_t bg);

#endif /* __SIMPLE_GFX_DRAW_H */
