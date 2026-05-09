#include "simple_gfx_draw.h"
#include "lcd_spi_port.h"
#include "main.h"
#include <ctype.h>

#define LCD_W 320
#define LCD_H 240

extern SPI_HandleTypeDef hspi1;

/* -----------------------------------------------------------------------
 * Font data — 5x7 bitmap, ASCII 32..90
 * ----------------------------------------------------------------------- */
static const uint8_t s_font5x7[][5] = {
{0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5F,0x00,0x00},{0x00,0x07,0x00,0x07,0x00},{0x14,0x7F,0x14,0x7F,0x14},
{0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,0x08,0x64,0x62},{0x36,0x49,0x55,0x22,0x50},{0x00,0x05,0x03,0x00,0x00},
{0x00,0x1C,0x22,0x41,0x00},{0x00,0x41,0x22,0x1C,0x00},{0x14,0x08,0x3E,0x08,0x14},{0x08,0x08,0x3E,0x08,0x08},
{0x00,0x50,0x30,0x00,0x00},{0x08,0x08,0x08,0x08,0x08},{0x00,0x60,0x60,0x00,0x00},{0x20,0x10,0x08,0x04,0x02},
{0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},{0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},
{0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
{0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1E},{0x00,0x36,0x36,0x00,0x00},{0x00,0x56,0x36,0x00,0x00},
{0x08,0x14,0x22,0x41,0x00},{0x14,0x14,0x14,0x14,0x14},{0x00,0x41,0x22,0x14,0x08},{0x02,0x01,0x51,0x09,0x06},
{0x32,0x49,0x79,0x41,0x3E},{0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
{0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},{0x3E,0x41,0x49,0x49,0x7A},
{0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},{0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},
{0x7F,0x40,0x40,0x40,0x40},{0x7F,0x02,0x0C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
{0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},{0x46,0x49,0x49,0x49,0x31},
{0x01,0x01,0x7F,0x01,0x01},{0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},
{0x63,0x14,0x08,0x14,0x63},{0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43}
};

/* -----------------------------------------------------------------------
 * Primitives
 * ----------------------------------------------------------------------- */

void GFX_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t c)
{
    if (x >= LCD_W || y >= LCD_H || w == 0 || h == 0) return;
    if (x + w > LCD_W) w = (uint16_t)(LCD_W - x);
    if (y + h > LCD_H) h = (uint16_t)(LCD_H - y);
    LCD_SetWindow(x, y, (uint16_t)(x + w - 1), (uint16_t)(y + h - 1));
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET);
    static uint8_t line_buf[640];
    for (int i = 0; i < 320; i++) {
        line_buf[i*2]   = (uint8_t)(c >> 8);
        line_buf[i*2+1] = (uint8_t)(c & 0xFF);
    }
    uint32_t pixels_left = (uint32_t)w * h;
    while (pixels_left > 0) {
        uint16_t chunk = (pixels_left > 320) ? 320 : (uint16_t)pixels_left;
        HAL_SPI_Transmit(&hspi1, line_buf, (uint16_t)(chunk * 2), 100);
        pixels_left -= chunk;
    }
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
}

static void draw_char5x7(uint16_t x, uint16_t y, char ch, uint16_t c, uint16_t bg)
{
    if (ch < 32 || ch > 90) ch = ' ';
    const uint8_t *g = s_font5x7[(uint8_t)ch - 32];
    LCD_SetWindow(x, y, (uint16_t)(x + 5), (uint16_t)(y + 7));
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET);
    uint8_t p_on[2]  = {(uint8_t)(c >> 8),  (uint8_t)c};
    uint8_t p_off[2] = {(uint8_t)(bg >> 8), (uint8_t)bg};
    for (uint8_t row = 0; row < 8; row++) {
        for (uint8_t col = 0; col < 6; col++) {
            if (col < 5 && row < 7 && (g[col] & (1U << row))) HAL_SPI_Transmit(&hspi1, p_on, 2, 10);
            else                                                HAL_SPI_Transmit(&hspi1, p_off, 2, 10);
        }
    }
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
}

void GFX_DrawText5x7(uint16_t x, uint16_t y, const char *s, uint16_t c, uint16_t bg)
{
    while (*s) {
        draw_char5x7(x, y, (char)toupper((unsigned char)*s++), c, bg);
        x = (uint16_t)(x + 6);
    }
}

void GFX_DrawText5x7Ellipsis(uint16_t x, uint16_t y, const char *s, uint16_t c, uint16_t bg, uint16_t max_chars)
{
    uint16_t len = 0U;
    while (s[len] != 0) len++;

    if (len <= max_chars) {
        GFX_DrawText5x7(x, y, s, c, bg);
        return;
    }

    char tmp[32];
    uint16_t keep = (max_chars > 3U) ? (uint16_t)(max_chars - 3U) : 0U;
    if (keep > (uint16_t)(sizeof(tmp) - 4U)) keep = (uint16_t)(sizeof(tmp) - 4U);
    for (uint16_t i = 0; i < keep; i++) tmp[i] = s[i];
    tmp[keep] = '.'; tmp[keep+1U] = '.'; tmp[keep+2U] = '.'; tmp[keep+3U] = 0;
    GFX_DrawText5x7(x, y, tmp, c, bg);
}

static void draw_char5x7_scale3_2(uint16_t x, uint16_t y, char ch, uint16_t c, uint16_t bg)
{
    if (ch < 32 || ch > 90) ch = ' ';
    const uint8_t *g = s_font5x7[(uint8_t)ch - 32];
    for (uint8_t dy = 0; dy < 12; dy++) {
        uint8_t sy = (uint8_t)((dy * 2) / 3);
        for (uint8_t dx = 0; dx < 9; dx++) {
            uint8_t sx = (uint8_t)((dx * 2) / 3);
            uint16_t px = (uint16_t)(x + dx);
            uint16_t py = (uint16_t)(y + dy);
            if (sx < 5 && sy < 7 && (g[sx] & (1U << sy))) GFX_FillRect(px, py, 1, 1, c);
            else                                            GFX_FillRect(px, py, 1, 1, bg);
        }
    }
}

void GFX_DrawText5x7Scale3_2(uint16_t x, uint16_t y, const char *s, uint16_t c, uint16_t bg)
{
    while (*s) {
        draw_char5x7_scale3_2(x, y, (char)toupper((unsigned char)*s++), c, bg);
        x = (uint16_t)(x + 9);
    }
}

void GFX_DrawTriangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, uint16_t c)
{
    int minx = x1, maxx = x1, miny = y1, maxy = y1;
    if (x2 < minx) minx = x2; if (x2 > maxx) maxx = x2;
    if (x3 < minx) minx = x3; if (x3 > maxx) maxx = x3;
    if (y2 < miny) miny = y2; if (y2 > maxy) maxy = y2;
    if (y3 < miny) miny = y3; if (y3 > maxy) maxy = y3;
    for (int y = miny; y <= maxy; y++) {
        for (int x = minx; x <= maxx; x++) {
            int d1 = (x-x1)*(y2-y1) - (y-y1)*(x2-x1);
            int d2 = (x-x2)*(y3-y2) - (y-y2)*(x3-x2);
            int d3 = (x-x3)*(y1-y3) - (y-y3)*(x1-x3);
            if ((d1>=0 && d2>=0 && d3>=0) || (d1<=0 && d2<=0 && d3<=0))
                GFX_FillRect((uint16_t)x, (uint16_t)y, 1, 1, c);
        }
    }
}

/* --- 7-segment helpers --- */
static void draw_seg_h(uint16_t x, uint16_t y, uint16_t len, uint16_t c)
{
    GFX_FillRect((uint16_t)(x+2U), y, len, 3, c);
    GFX_DrawTriangle(x, (uint16_t)(y+1U), (uint16_t)(x+2U), y, (uint16_t)(x+2U), (uint16_t)(y+2U), c);
    GFX_DrawTriangle((uint16_t)(x+2U+len), y, (uint16_t)(x+2U+len+2U), (uint16_t)(y+1U), (uint16_t)(x+2U+len), (uint16_t)(y+2U), c);
}

static void draw_seg_v(uint16_t x, uint16_t y, uint16_t len, uint16_t c)
{
    GFX_FillRect(x, (uint16_t)(y+2U), 3, len, c);
    GFX_DrawTriangle((uint16_t)(x+1U), y, x, (uint16_t)(y+2U), (uint16_t)(x+2U), (uint16_t)(y+2U), c);
    GFX_DrawTriangle(x, (uint16_t)(y+2U+len), (uint16_t)(x+1U), (uint16_t)(y+2U+len+2U), (uint16_t)(x+2U), (uint16_t)(y+2U+len), c);
}

static void draw_dseg_diamond_char(uint16_t x, uint16_t y, char ch, uint16_t c)
{
    static const uint8_t map[10] = {0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F};
    if (ch == ':') {
        GFX_FillRect((uint16_t)(x+4U), (uint16_t)(y+6U),  2, 2, c);
        GFX_FillRect((uint16_t)(x+4U), (uint16_t)(y+14U), 2, 2, c);
        return;
    }
    if (ch < '0' || ch > '9') return;
    uint8_t bits = map[ch - '0'];
    if (bits & 0x01U) draw_seg_h((uint16_t)(x+2U), y, 7, c);
    if (bits & 0x02U) draw_seg_v((uint16_t)(x+12U), (uint16_t)(y+2U),  5, c);
    if (bits & 0x04U) draw_seg_v((uint16_t)(x+12U), (uint16_t)(y+12U), 5, c);
    if (bits & 0x08U) draw_seg_h((uint16_t)(x+2U),  (uint16_t)(y+20U), 7, c);
    if (bits & 0x10U) draw_seg_v(x, (uint16_t)(y+12U), 5, c);
    if (bits & 0x20U) draw_seg_v(x, (uint16_t)(y+2U),  5, c);
    if (bits & 0x40U) draw_seg_h((uint16_t)(x+2U),  (uint16_t)(y+10U), 7, c);
}

void GFX_DrawDseg16Text(uint16_t x, uint16_t y, const char *s, uint16_t c, uint16_t bg)
{
    (void)bg;
    while (*s) {
        draw_dseg_diamond_char(x, (uint16_t)(y - 22U), *s, c);
        x = (uint16_t)(x + ((*s == ':') ? 8U : 18U));
        s++;
    }
}
