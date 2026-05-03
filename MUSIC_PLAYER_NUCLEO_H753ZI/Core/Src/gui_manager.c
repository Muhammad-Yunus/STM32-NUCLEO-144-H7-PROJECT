#include "gui_manager.h"
#include "ili9341.h"
#include "main.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#define LCD_W 320
#define LCD_H 240

/* Winamp-inspired Colors from M5Mp3 */
#define C_BG      0x2104  /* grays[15] approximation */
#define C_BLACK   0x0000
#define C_ORANGE  0xFD20
#define C_GREEN   0x07E0
#define C_WHITE   0xFFFF
#define C_GRAY_L  0x8410  /* light / grays[11] */
#define C_GRAY_D  0x4208  /* grays[4] approximation */
#define C_YELLOW  0xFFE0
#define C_RED     0xF800
#define C_MAGENTA 0xF81F

#define UI_LIST_X 5
#define UI_LIST_Y 14
#define UI_LIST_W 172
#define UI_LIST_H 216
#define UI_SCROLL_X 177
#define UI_SCROLL_W 7
#define UI_SCROLL_THUMB_H 28
#define UI_RIGHT_X 197
#define UI_STATUS_Y 25
#define UI_STATUS_W 115
#define UI_STATUS_H 75
#define UI_TRACK_Y 105
#define UI_TRACK_W 115
#define UI_TRACK_H 28
#define UI_BTN_Y 167
#define UI_BTN_W 24
#define UI_BTN_H 32
#define UI_BTN_GAP 29
#define UI_BTN0_X 197
#define UI_VIZ_X 229
#define UI_VIZ_BASE_Y 88
#define UI_VIZ_PITCH 5
#define UI_LIST_ROW_H 21
#define UI_VOL_Y 146
#define UI_LIG_Y 221
extern SPI_HandleTypeDef hspi1;
extern void LCD_WriteCmd(uint8_t cmd);
extern void LCD_WriteData(uint8_t data);
extern void LCD_WriteMultiData(uint8_t *data, size_t length);

static char s_track_name[64] = "No track";
static char s_last_track_name[64] = "";
static uint8_t s_is_playing = 0U;
static uint8_t s_last_playing = 0xFF;
static const char *s_tracks[64];
static uint16_t s_track_count = 0;
static int s_selected = -1;
static int s_last_selected = -1;
static uint8_t s_prev_pressed = 0;
static uint8_t s_viz_heights[14] = {0};
static uint8_t s_viz_smoothed[14] = {0};
static uint8_t s_viz_prev_heights[14] = {0};
static uint16_t s_volume = 10; /* percent, 0..100 step 10 */
static uint8_t s_brightness = 4;
static uint8_t s_slider_mode = 0; /* 0 none, 1 vol, 2 lig */

__attribute__((weak)) void GuiManager_OnVolumeChange(uint16_t volume) { (void)volume; }
__attribute__((weak)) void GuiManager_OnBrightnessChange(uint8_t level) { (void)level; }
__attribute__((weak)) void GuiManager_OnRefreshPlaylist(void) {}
static uint8_t s_battery_percent = 100;
static uint16_t s_time_mm = 0;
static uint16_t s_time_ss = 0;
static uint16_t s_last_time_mm = 0xFFFF;
static uint16_t s_last_time_ss = 0xFFFF;
static uint16_t s_scroll_pos = 0;
static uint16_t s_list_top = 0;
static int16_t s_prev_touch_y = -1;
static uint8_t s_list_dirty = 1U;
static uint16_t s_list_drawn_top = 0xFFFF;
static int s_list_drawn_selected = -2;
static uint16_t s_list_drawn_count = 0xFFFF;
static uint8_t s_list_drawn_scroll = 0xFF;
static uint8_t s_list_drawn_playing = 0xFF;
static char s_list_last_track_name[64] = "";
static uint16_t s_track_marquee_offset = 0;
static uint32_t s_track_marquee_last_ms = 0;
static void draw_playlist_rows(void);
static void update_scroll_indicator(void);
static uint16_t list_visible_rows(void);
static uint16_t list_max_top(void);
static uint16_t touch_to_row(uint16_t y);
static void ensure_selected_visible(void);
static void draw_spectrum_bars(uint8_t decay_step);

static void fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t c);
static void draw_text5x7(uint16_t x, uint16_t y, const char *s, uint16_t c, uint16_t bg);
static void draw_triangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, uint16_t c);

static int8_t s_pressed_button = -1;
static int8_t s_last_pressed_button = -2;  /* force initial draw */
static uint8_t s_last_playing_icon = 0xFF; /* force initial draw */
static uint8_t s_shuffle_enabled = 0U;
static uint8_t s_last_shuffle_enabled = 0xFF;

static void draw_transport_buttons(void)
{
    for (int i = 0; i < 4; i++) {
        uint16_t x = (uint16_t)(UI_BTN0_X + i * UI_BTN_GAP);
        uint16_t y = UI_BTN_Y;
        uint16_t face = (s_pressed_button == i) ? 0x3186 : C_GRAY_D;
        uint16_t hi = C_GRAY_L;
        uint16_t lo = 0x2104;

        fill_rect(x, y, UI_BTN_W, UI_BTN_H, face);
        if (s_pressed_button == i) {
            fill_rect(x, y, UI_BTN_W, 1, lo);
            fill_rect(x, y, 1, UI_BTN_H, lo);
            fill_rect(x, (uint16_t)(y + UI_BTN_H - 1), UI_BTN_W, 1, hi);
            fill_rect((uint16_t)(x + UI_BTN_W - 1), y, 1, UI_BTN_H, hi);
        } else {
            fill_rect(x, y, UI_BTN_W, 1, hi);
            fill_rect(x, y, 1, UI_BTN_H, hi);
            fill_rect(x, (uint16_t)(y + UI_BTN_H - 1), UI_BTN_W, 1, lo);
            fill_rect((uint16_t)(x + UI_BTN_W - 1), y, 1, UI_BTN_H, lo);
        }
    }

    draw_text5x7(205, 171, "A", C_BLACK, C_GRAY_D);
    draw_text5x7(234, 171, "P", C_BLACK, C_GRAY_D);
    draw_text5x7(263, 171, "N", C_BLACK, C_GRAY_D);
    draw_text5x7(292, 171, "B", C_BLACK, C_GRAY_D);

    int oA = (s_pressed_button == 0) ? 1 : 0;
    int oP = (s_pressed_button == 1) ? 1 : 0;
    int oN = (s_pressed_button == 2) ? 1 : 0;
    int oB = (s_pressed_button == 3) ? 1 : 0;

    if (s_is_playing) fill_rect((uint16_t)(204 + oA), (uint16_t)(181 + oA), 8, 10, C_GRAY_L);
    else { fill_rect((uint16_t)(204 + oA), (uint16_t)(181 + oA), 8, 8, C_GRAY_D); draw_triangle((uint16_t)(204 + oA), (uint16_t)(181 + oA), (uint16_t)(204 + oA), (uint16_t)(191 + oA), (uint16_t)(211 + oA), (uint16_t)(186 + oA), C_GRAY_L); }

    draw_triangle((uint16_t)(229 + oP), (uint16_t)(186 + oP), (uint16_t)(236 + oP), (uint16_t)(181 + oP), (uint16_t)(236 + oP), (uint16_t)(191 + oP), C_GRAY_L);
    draw_triangle((uint16_t)(236 + oP), (uint16_t)(186 + oP), (uint16_t)(243 + oP), (uint16_t)(181 + oP), (uint16_t)(243 + oP), (uint16_t)(191 + oP), C_GRAY_L);

    draw_triangle((uint16_t)(262 + oN), (uint16_t)(181 + oN), (uint16_t)(262 + oN), (uint16_t)(191 + oN), (uint16_t)(269 + oN), (uint16_t)(186 + oN), C_GRAY_L);
    draw_triangle((uint16_t)(269 + oN), (uint16_t)(181 + oN), (uint16_t)(269 + oN), (uint16_t)(191 + oN), (uint16_t)(276 + oN), (uint16_t)(186 + oN), C_GRAY_L);

    fill_rect((uint16_t)(291 + oB), (uint16_t)(183 + oB), 10, 2, C_GRAY_L);
    draw_triangle((uint16_t)(301 + oB), (uint16_t)(181 + oB), (uint16_t)(301 + oB), (uint16_t)(187 + oB), (uint16_t)(305 + oB), (uint16_t)(184 + oB), C_GRAY_L);
    fill_rect((uint16_t)(291 + oB), (uint16_t)(190 + oB), 10, 2, C_GRAY_L);
    draw_triangle((uint16_t)(291 + oB), (uint16_t)(188 + oB), (uint16_t)(291 + oB), (uint16_t)(194 + oB), (uint16_t)(287 + oB), (uint16_t)(191 + oB), C_GRAY_L);
}

static const uint8_t font5x7[][5] = {
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

static void lcd_set_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    uint8_t d[4];
    LCD_WriteCmd(ILI9341_CMD_COLADDR);
    d[0] = (uint8_t)(x1 >> 8); d[1] = (uint8_t)x1; d[2] = (uint8_t)(x2 >> 8); d[3] = (uint8_t)x2;
    LCD_WriteMultiData(d, 4);
    LCD_WriteCmd(ILI9341_CMD_PAGEADDR);
    d[0] = (uint8_t)(y1 >> 8); d[1] = (uint8_t)y1; d[2] = (uint8_t)(y2 >> 8); d[3] = (uint8_t)y2;
    LCD_WriteMultiData(d, 4);
    LCD_WriteCmd(ILI9341_CMD_GRAM);
}

static void fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t c)
{
    if (x >= LCD_W || y >= LCD_H || w == 0 || h == 0) return;
    if (x + w > LCD_W) w = LCD_W - x;
    if (y + h > LCD_H) h = LCD_H - y;
    lcd_set_window(x, y, (uint16_t)(x + w - 1), (uint16_t)(y + h - 1));
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET);
    static uint8_t line_buf[640];
    for (int i = 0; i < 320; i++) {
        line_buf[i*2] = (uint8_t)(c >> 8);
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
    const uint8_t *g = font5x7[(uint8_t)ch - 32];
    lcd_set_window(x, y, (uint16_t)(x + 5), (uint16_t)(y + 7));
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET);
    uint8_t p_on[2] = {(uint8_t)(c >> 8), (uint8_t)c};
    uint8_t p_off[2] = {(uint8_t)(bg >> 8), (uint8_t)bg};
    for (uint8_t row = 0; row < 8; row++) {
        for (uint8_t col = 0; col < 6; col++) {
            if (col < 5 && row < 7 && (g[col] & (1U << row))) HAL_SPI_Transmit(&hspi1, p_on, 2, 10);
            else HAL_SPI_Transmit(&hspi1, p_off, 2, 10);
        }
    }
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
}

static void draw_text5x7(uint16_t x, uint16_t y, const char *s, uint16_t c, uint16_t bg)
{
    while (*s) {
        draw_char5x7(x, y, (char)toupper((unsigned char)*s++), c, bg);
        x += 6;
    }
}

static void draw_text5x7_ellipsis(uint16_t x, uint16_t y, const char *s, uint16_t c, uint16_t bg, uint16_t max_chars)
{
    uint16_t len = 0U;
    while (s[len] != 0) len++;

    if (len <= max_chars) {
        draw_text5x7(x, y, s, c, bg);
        return;
    }

    char tmp[32];
    uint16_t keep = (max_chars > 3U) ? (uint16_t)(max_chars - 3U) : 0U;
    if (keep > (uint16_t)(sizeof(tmp) - 4U)) keep = (uint16_t)(sizeof(tmp) - 4U);

    for (uint16_t i = 0; i < keep; i++) tmp[i] = s[i];
    tmp[keep] = '.';
    tmp[keep + 1U] = '.';
    tmp[keep + 2U] = '.';
    tmp[keep + 3U] = 0;
    draw_text5x7(x, y, tmp, c, bg);
}

static void draw_char5x7_scaled(uint16_t x, uint16_t y, char ch, uint8_t scale, uint16_t c, uint16_t bg)
{
    if (scale == 0) return;
    if (ch < 32 || ch > 90) ch = ' ';
    const uint8_t *g = font5x7[(uint8_t)ch - 32];
    for (uint8_t row = 0; row < 8; row++) {
        for (uint8_t col = 0; col < 6; col++) {
            uint16_t px = (uint16_t)(x + col * scale);
            uint16_t py = (uint16_t)(y + row * scale);
            if (col < 5 && row < 7 && (g[col] & (1U << row))) fill_rect(px, py, scale, scale, c);
            else fill_rect(px, py, scale, scale, bg);
        }
    }
}

static void draw_text5x7_scaled(uint16_t x, uint16_t y, const char *s, uint8_t scale, uint16_t c, uint16_t bg)
{
    while (*s) {
        draw_char5x7_scaled(x, y, (char)toupper((unsigned char)*s++), scale, c, bg);
        x = (uint16_t)(x + 6 * scale);
    }
}

static void draw_char5x7_scale3_2(uint16_t x, uint16_t y, char ch, uint16_t c, uint16_t bg)
{
    if (ch < 32 || ch > 90) ch = ' ';
    const uint8_t *g = font5x7[(uint8_t)ch - 32];
    for (uint8_t dy = 0; dy < 12; dy++) {
        uint8_t sy = (uint8_t)((dy * 2) / 3);
        for (uint8_t dx = 0; dx < 9; dx++) {
            uint8_t sx = (uint8_t)((dx * 2) / 3);
            uint16_t px = (uint16_t)(x + dx);
            uint16_t py = (uint16_t)(y + dy);
            if (sx < 5 && sy < 7 && (g[sx] & (1U << sy))) fill_rect(px, py, 1, 1, c);
            else fill_rect(px, py, 1, 1, bg);
        }
    }
}

static void draw_text5x7_scale3_2(uint16_t x, uint16_t y, const char *s, uint16_t c, uint16_t bg)
{
    while (*s) {
        draw_char5x7_scale3_2(x, y, (char)toupper((unsigned char)*s++), c, bg);
        x = (uint16_t)(x + 9);
    }
}

static void draw_triangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, uint16_t c)
{
    int minx = x1, maxx = x1, miny = y1, maxy = y1;
    if (x2 < minx) minx = x2; if (x2 > maxx) maxx = x2;
    if (x3 < minx) minx = x3; if (x3 > maxx) maxx = x3;
    if (y2 < miny) miny = y2; if (y2 > maxy) maxy = y2;
    if (y3 < miny) miny = y3; if (y3 > maxy) maxy = y3;
    for (int y = miny; y <= maxy; y++) {
        for (int x = minx; x <= maxx; x++) {
            int d1 = (x - x1) * (y2 - y1) - (y - y1) * (x2 - x1);
            int d2 = (x - x2) * (y3 - y2) - (y - y2) * (x3 - x2);
            int d3 = (x - x3) * (y1 - y3) - (y - y3) * (x1 - x3);
            if ((d1 >= 0 && d2 >= 0 && d3 >= 0) || (d1 <= 0 && d2 <= 0 && d3 <= 0)) fill_rect((uint16_t)x, (uint16_t)y, 1, 1, c);
        }
    }
}

static const uint8_t dseg16_digit_bitmap[][20] = {
    {0x7F,0x00,0x08,0x0A,0x02,0x80,0xA0,0x28,0x08,0x00,0x00,0x20,0x28,0x0A,0x02,0x80,0xA0,0x20,0x01,0xFC},
    {0x2A,0xA0,0xAA,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x7F,0x00,0x00,0x08,0x02,0x00,0x80,0x20,0x09,0xFC,0x7F,0x20,0x08,0x02,0x00,0x80,0x20,0x00,0x01,0xFC},
    {0xFE,0x00,0x00,0x40,0x20,0x10,0x08,0x05,0xFC,0xFE,0x00,0x80,0x40,0x20,0x10,0x08,0x01,0xFC,0x00,0x00},
    {0x00,0x20,0x28,0x0A,0x02,0x80,0xA0,0x27,0xF1,0xFC,0x00,0x80,0x20,0x08,0x02,0x00,0x80,0x00,0x00,0x00},
    {0x7F,0x00,0x08,0x02,0x00,0x80,0x20,0x08,0x01,0xFC,0x7F,0x00,0x20,0x08,0x02,0x00,0x80,0x20,0x01,0xFC},
    {0x7F,0x00,0x08,0x02,0x00,0x80,0x20,0x08,0x01,0xFC,0x7F,0x20,0x28,0x0A,0x02,0x80,0xA0,0x20,0x01,0xFC},
    {0x7F,0x00,0x08,0x0A,0x02,0x80,0xA0,0x28,0x08,0x00,0x00,0x00,0x20,0x08,0x02,0x00,0x80,0x20,0x00,0x00},
    {0x7F,0x00,0x08,0x0A,0x02,0x80,0xA0,0x28,0x09,0xFC,0x7F,0x20,0x28,0x0A,0x02,0x80,0xA0,0x20,0x01,0xFC},
    {0x7F,0x00,0x08,0x0A,0x02,0x80,0xA0,0x28,0x09,0xFC,0x7F,0x00,0x20,0x08,0x02,0x00,0x80,0x20,0x01,0xFC}
};
static const uint8_t dseg16_colon_bitmap[4] = {0xD0,0x00,0x06,0x00};
static const uint8_t dseg16_widths[11] = {10,2,10,9,10,10,10,10,10,10,3};
static const uint8_t dseg16_heights[11] = {16,14,16,16,14,16,16,15,16,16,9};
static const uint8_t dseg16_advance[11] = {14,14,14,14,14,14,14,14,14,14,4};
static const int8_t dseg16_xoff[11] = {2,10,2,3,2,2,2,2,2,2,1};
static const int8_t dseg16_yoff[11] = {-16,-15,-16,-16,-15,-16,-16,-16,-16,-16,-12};
static const uint16_t dseg16_bit_offset[11] = {0,160,188,348,492,632,792,952,1102,1262,1422};
static uint8_t dseg16_stream_bit(uint16_t bit_index, int idx)
{
    if (idx == 10) {
        uint8_t b = dseg16_colon_bitmap[bit_index >> 3];
        return (uint8_t)((b >> (7 - (bit_index & 7))) & 1U);
    }
    uint8_t b = dseg16_digit_bitmap[idx][bit_index >> 3];
    return (uint8_t)((b >> (7 - (bit_index & 7))) & 1U);
}

static void draw_dseg16_char(uint16_t x, uint16_t y, char ch, uint16_t c, uint16_t bg)
{
    int idx = -1;
    if (ch >= '0' && ch <= '9') idx = ch - '0';
    else if (ch == ':') idx = 10;
    if (idx < 0) return;

    uint8_t w = dseg16_widths[idx];
    uint8_t h = dseg16_heights[idx];
    int16_t gx = (int16_t)x + dseg16_xoff[idx];
    int16_t gy = (int16_t)y + dseg16_yoff[idx];
    uint16_t start_bit = dseg16_bit_offset[idx];

    for (uint8_t row = 0; row < h; row++) {
        for (uint8_t col = 0; col < w; col++) {
            uint16_t bit = (uint16_t)(start_bit + row * w + col);
            uint16_t px = (uint16_t)(gx + col);
            uint16_t py = (uint16_t)(gy + row);
            fill_rect(px, py, 1, 1, dseg16_stream_bit(bit - start_bit, idx) ? c : bg);
        }
    }
}

static void draw_dseg16_text(uint16_t x, uint16_t y, const char *s, uint16_t c, uint16_t bg)
{
    while (*s) {
        int idx = (*s >= '0' && *s <= '9') ? (*s - '0') : ((*s == ':') ? 10 : -1);
        if (idx >= 0) draw_dseg16_char(x, y, *s, c, bg);
        x = (uint16_t)(x + ((idx >= 0) ? dseg16_advance[idx] : 6));
        s++;
    }
}

static void draw_seg_h(uint16_t x, uint16_t y, uint16_t len, uint16_t c)
{
    fill_rect((uint16_t)(x + 2U), y, len, 3, c);
    draw_triangle(x, (uint16_t)(y + 1U), (uint16_t)(x + 2U), y, (uint16_t)(x + 2U), (uint16_t)(y + 2U), c);
    draw_triangle((uint16_t)(x + 2U + len), y, (uint16_t)(x + 2U + len + 2U), (uint16_t)(y + 1U), (uint16_t)(x + 2U + len), (uint16_t)(y + 2U), c);
}

static void draw_seg_v(uint16_t x, uint16_t y, uint16_t len, uint16_t c)
{
    fill_rect(x, (uint16_t)(y + 2U), 3, len, c);
    draw_triangle((uint16_t)(x + 1U), y, x, (uint16_t)(y + 2U), (uint16_t)(x + 2U), (uint16_t)(y + 2U), c);
    draw_triangle(x, (uint16_t)(y + 2U + len), (uint16_t)(x + 1U), (uint16_t)(y + 2U + len + 2U), (uint16_t)(x + 2U), (uint16_t)(y + 2U + len), c);
}

static void draw_dseg_diamond_char(uint16_t x, uint16_t y, char ch, uint16_t c)
{
    static const uint8_t map[10] = {0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F};
    if (ch == ':') {
        fill_rect((uint16_t)(x + 4U), (uint16_t)(y + 6U), 2, 2, c);
        fill_rect((uint16_t)(x + 4U), (uint16_t)(y + 14U), 2, 2, c);
        return;
    }
    if (ch < '0' || ch > '9') return;

    uint8_t bits = map[ch - '0'];
    if (bits & 0x01U) draw_seg_h((uint16_t)(x + 2U), y, 7, c);
    if (bits & 0x02U) draw_seg_v((uint16_t)(x + 12U), (uint16_t)(y + 2U), 5, c);
    if (bits & 0x04U) draw_seg_v((uint16_t)(x + 12U), (uint16_t)(y + 12U), 5, c);
    if (bits & 0x08U) draw_seg_h((uint16_t)(x + 2U), (uint16_t)(y + 20U), 7, c);
    if (bits & 0x10U) draw_seg_v(x, (uint16_t)(y + 12U), 5, c);
    if (bits & 0x20U) draw_seg_v(x, (uint16_t)(y + 2U), 5, c);
    if (bits & 0x40U) draw_seg_h((uint16_t)(x + 2U), (uint16_t)(y + 10U), 7, c);
}

static void draw_dseg16_text_scale6_5_bold3(uint16_t x, uint16_t y, const char *s, uint16_t c, uint16_t bg)
{
    (void)bg;
    while (*s) {
        draw_dseg_diamond_char(x, (uint16_t)(y - 22U), *s, c);
        x = (uint16_t)(x + ((*s == ':') ? 8U : 18U));
        s++;
    }
}

static void draw_static(void)
{
    fill_rect(0, 0, 320, 240, C_BG);
    fill_rect(UI_LIST_X, UI_LIST_Y, UI_LIST_W, UI_LIST_H, C_BLACK);
    fill_rect(UI_SCROLL_X, UI_LIST_Y, UI_SCROLL_W, UI_LIST_H, 0x0841);
    fill_rect(UI_SCROLL_X + 2, UI_LIST_Y + 4, 1, 24, 0xC618);

    fill_rect(5, 7, 67, 2, C_ORANGE);
    fill_rect(112, 7, 67, 2, C_ORANGE);
    fill_rect(253, 4, 57, 2, C_ORANGE);
    fill_rect(253, 11, 57, 4, C_GRAY_D);

    fill_rect(4, 11, 1, 218, C_GRAY_L);
    fill_rect(182, 11, 1, 218, C_GRAY_L);
    fill_rect(4, 229, 178, 1, C_GRAY_L);
    fill_rect(0, 0, 320, 1, C_GRAY_L);
    fill_rect(0, 239, 320, 1, C_GRAY_L);
    fill_rect(0, 0, 1, 240, C_GRAY_L);
    fill_rect(319, 0, 1, 240, C_GRAY_L);
    fill_rect(184, 0, 1, 240, C_GRAY_L);
    fill_rect(185, 0, 4, 240, C_BLACK);
    fill_rect(190, 0, 1, 240, C_GRAY_L);

    fill_rect(195, 20, UI_STATUS_W, UI_STATUS_H, C_BLACK);
    fill_rect(195, UI_TRACK_Y, UI_TRACK_W, UI_TRACK_H, C_BLACK);
    fill_rect(195, 20, 1, UI_STATUS_H, C_GRAY_L);
    fill_rect(195, 20, UI_STATUS_W, 1, C_GRAY_L);

    draw_transport_buttons();

    fill_rect(229, UI_VOL_Y, 80, 4, C_YELLOW);
    fill_rect(245, UI_VOL_Y - 3, 13, 10, C_GRAY_L);
    fill_rect(247, UI_VOL_Y - 1, 9, 6, C_GRAY_D);

    fill_rect(229, UI_LIG_Y, 40, 4, C_MAGENTA);
    fill_rect(229, UI_LIG_Y - 3, 13, 10, C_GRAY_L);
    fill_rect(231, UI_LIG_Y - 1, 9, 6, C_GRAY_D);

    fill_rect(277, 214, 33, 18, C_BG);
    fill_rect(277, 214, 33, 1, C_GREEN);
    fill_rect(277, 231, 33, 1, C_GREEN);
    fill_rect(277, 214, 1, 18, C_GREEN);
    fill_rect(309, 214, 1, 18, C_GREEN);
    fill_rect(310, 219, 4, 8, C_GREEN);

    draw_text5x7_scale3_2(75, 2, "LIST", C_GRAY_L, C_BG);
    draw_text5x7_scale3_2(196, 4, "WINAMP", C_GRAY_L, C_BG);
    draw_text5x7(200, 143, "VOL", C_GRAY_L, C_BG);
    draw_text5x7(200, 218, "LIG", C_GRAY_L, C_BG);

    draw_dseg16_text_scale6_5_bold3(232, 48, "00:00", C_GREEN, C_BLACK);
    draw_text5x7(282, 219, "100%", C_GREEN, C_BG);
}

void GuiManager_Init(void) { draw_static(); s_list_dirty = 1U; }
void GuiManager_ClearPlaylist(void) { s_track_count = 0; s_selected = -1; s_list_top = 0; draw_static(); memset(s_viz_prev_heights, 0, sizeof(s_viz_prev_heights)); s_list_dirty = 1U; }
void GuiManager_AddTrackToList(const char *name) { if (s_track_count < 64) { s_tracks[s_track_count++] = name; s_list_dirty = 1U; } }

static uint16_t list_visible_rows(void)
{
    return (uint16_t)(UI_LIST_H / UI_LIST_ROW_H);
}

static uint16_t list_max_top(void)
{
    uint16_t vis = list_visible_rows();
    if (s_track_count <= vis) return 0;
    return (uint16_t)(s_track_count - vis);
}

static uint16_t touch_to_row(uint16_t y)
{
    uint16_t rel = (uint16_t)(y - UI_LIST_Y);
    uint16_t row = (uint16_t)(rel / UI_LIST_ROW_H);
    uint16_t vis = list_visible_rows();
    if (row >= vis) row = (uint16_t)(vis - 1U);
    return row;
}

static void ensure_selected_visible(void)
{
    if (s_selected < 0) return;
    uint16_t vis = list_visible_rows();
    uint16_t sel = (uint16_t)s_selected;
    if (sel < s_list_top) s_list_top = sel;
    if (sel >= (uint16_t)(s_list_top + vis)) s_list_top = (uint16_t)(sel - vis + 1U);
    uint16_t max_top = list_max_top();
    if (s_list_top > max_top) s_list_top = max_top;
}

static void update_scroll_indicator(void)
{
    if (s_track_count > 0) {
        uint16_t max_top = list_max_top();
        if (max_top == 0U) s_scroll_pos = 0;
        else s_scroll_pos = (uint16_t)((uint32_t)s_list_top * (UI_LIST_H - UI_SCROLL_THUMB_H) / max_top);
    } else {
        s_scroll_pos = 0;
    }

    fill_rect(UI_SCROLL_X, UI_LIST_Y, UI_SCROLL_W, UI_LIST_H, 0x0841);
    fill_rect(UI_SCROLL_X, UI_LIST_Y + s_scroll_pos, UI_SCROLL_W, UI_SCROLL_THUMB_H, 0xC618);
    fill_rect(UI_SCROLL_X + 2, UI_LIST_Y + s_scroll_pos + 7, 1, UI_SCROLL_THUMB_H - 14, C_GRAY_L);
}

static void draw_playlist_rows(void)
{
    uint16_t vis = list_visible_rows();
    for (uint16_t row = 0; row < vis; row++) {
        uint16_t idx = (uint16_t)(s_list_top + row);
        uint16_t y = (uint16_t)(18 + row * UI_LIST_ROW_H);
        fill_rect(10, y, 166, 8, C_BLACK);
        if (idx < s_track_count && s_tracks[idx] != NULL) {
            uint16_t tc = (s_selected == (int)idx) ? C_WHITE : C_GREEN;
            draw_text5x7_ellipsis(10, y, s_tracks[idx], tc, C_BLACK, 27U);
        }
    }
}

static void draw_spectrum_bars(uint8_t decay_step)
{
    for (int i = 0; i < 14; i++) {
        uint8_t l = (i > 0) ? s_viz_heights[i - 1] : s_viz_heights[i];
        uint8_t c = s_viz_heights[i];
        uint8_t r = (i < 13) ? s_viz_heights[i + 1] : s_viz_heights[i];
        uint8_t v = (uint8_t)((l + (c * 2U) + r + 2U) / 4U);
        if (i > 0 && i < 13 && v == 0U && l > 0U && r > 0U) {
            v = 1U;
        }
        if (v > 8U) v = 8U;
        s_viz_smoothed[i] = v;
    }

    fill_rect(UI_VIZ_X, 50, 69, 41, C_BLACK);
    for (int i = 0; i < 14; i++) {
        uint8_t target = s_viz_smoothed[i];
        uint8_t ph = s_viz_prev_heights[i];
        uint8_t h = target;
        if (target < ph) {
            uint8_t d = (decay_step == 0U) ? 1U : decay_step;
            h = (ph > d) ? (uint8_t)(ph - d) : 0U;
            if (h < target) h = target;
        }
        uint16_t x = (uint16_t)(UI_VIZ_X + i * UI_VIZ_PITCH);
        for (int j = 0; j < h; j++) fill_rect(x, (uint16_t)(UI_VIZ_BASE_Y - j * 5), 4, 3, C_GRAY_D);
        s_viz_prev_heights[i] = h;
    }
}

void GuiManager_Update(void)
{
    ensure_selected_visible();
    update_scroll_indicator();

    uint16_t track_max_chars = (uint16_t)((UI_TRACK_W - 2U) / 6U);
    uint16_t track_len = 0U;
    while (s_track_name[track_len] != 0) track_len++;
    if (track_len > track_max_chars) {
        uint32_t now = HAL_GetTick();
        if ((now - s_track_marquee_last_ms) >= 180U) {
            s_track_marquee_last_ms = now;
            s_track_marquee_offset++;
            if (s_track_marquee_offset > track_len) s_track_marquee_offset = 0U;
        }
    } else {
        s_track_marquee_offset = 0U;
        s_track_marquee_last_ms = HAL_GetTick();
    }


    if (s_list_dirty || s_list_drawn_top != s_list_top || s_list_drawn_selected != s_selected || s_list_drawn_count != s_track_count || s_list_drawn_scroll != s_scroll_pos || s_list_drawn_playing != s_is_playing || strcmp(s_list_last_track_name, s_track_name) != 0) {
        draw_playlist_rows();
        s_list_drawn_top = s_list_top;
        s_list_drawn_selected = s_selected;
        s_list_drawn_count = s_track_count;
        s_list_drawn_scroll = (uint8_t)s_scroll_pos;
        s_list_drawn_playing = s_is_playing;
        strncpy(s_list_last_track_name, s_track_name, sizeof(s_list_last_track_name));
        s_list_last_track_name[sizeof(s_list_last_track_name) - 1U] = 0;
        s_list_dirty = 0U;
    }

    if (s_is_playing != s_last_playing) {
        fill_rect(205, 28, 26, 48, C_BLACK);
        if (s_is_playing) {
            draw_triangle(215, 31, 215, 47, 230, 39, C_GREEN);
            draw_text5x7_scale3_2(200, 30, "P", C_GRAY_D, C_BLACK);
            draw_text5x7_scale3_2(200, 43, "L", C_GRAY_D, C_BLACK);
            draw_text5x7_scale3_2(200, 56, "A", C_GRAY_D, C_BLACK);
            draw_text5x7_scale3_2(200, 69, "Y", C_GRAY_D, C_BLACK);
            fill_rect(204, 181, 8, 10, C_GRAY_L);
        } else {
            fill_rect(215, 31, 16, 16, C_RED);
            draw_text5x7_scale3_2(200, 30, "S", C_GRAY_D, C_BLACK);
            draw_text5x7_scale3_2(200, 43, "T", C_GRAY_D, C_BLACK);
            draw_text5x7_scale3_2(200, 56, "O", C_GRAY_D, C_BLACK);
            draw_text5x7_scale3_2(200, 69, "P", C_GRAY_D, C_BLACK);
            fill_rect(204, 181, 8, 8, C_GRAY_D);
            draw_triangle(204, 181, 204, 191, 211, 186, C_GRAY_L);
        }
        s_last_playing = s_is_playing;
    }

    if (s_time_mm != s_last_time_mm || s_time_ss != s_last_time_ss) {
        char t[6];
        t[0] = (char)('0' + ((s_time_mm / 10) % 10));
        t[1] = (char)('0' + (s_time_mm % 10));
        t[2] = ':';
        t[3] = (char)('0' + ((s_time_ss / 10) % 10));
        t[4] = (char)('0' + (s_time_ss % 10));
        t[5] = 0;
        fill_rect(230, 26, 80, 24, C_BLACK);
        draw_dseg16_text_scale6_5_bold3(232, 48, t, C_GREEN, C_BLACK);
        s_last_time_mm = s_time_mm;
        s_last_time_ss = s_time_ss;
    }

    {
        uint8_t redraw_track = 0U;
        if (strcmp(s_track_name, s_last_track_name) != 0) {
            redraw_track = 1U;
            s_track_marquee_offset = 0U;
            strncpy(s_last_track_name, s_track_name, 64);
            s_last_track_name[63] = 0;
        }
        if (track_len > track_max_chars) {
            redraw_track = 1U;
        }
        if (redraw_track) {
            char shown[64];
            if (track_len <= track_max_chars) {
                strncpy(shown, s_track_name, sizeof(shown));
                shown[sizeof(shown) - 1U] = 0;
            } else {
                uint16_t span = (uint16_t)(track_len + 3U);
                uint16_t start = (span > 0U) ? (uint16_t)(s_track_marquee_offset % span) : 0U;
                for (uint16_t i = 0; i < track_max_chars; i++) {
                    uint16_t pos = (uint16_t)(start + i);
                    if (pos >= span) pos = (uint16_t)(pos - span);
                    if (pos < track_len) shown[i] = s_track_name[pos];
                    else shown[i] = ' ';
                }
                shown[track_max_chars] = 0;
            }
            fill_rect(198, 106, (uint16_t)(UI_TRACK_W - 4), 14, C_BLACK);
            draw_text5x7(198, 115, shown, C_GREEN, C_BLACK);
        }
    }


    draw_spectrum_bars(1U);

    fill_rect(206, 85, 14, 8, C_BLACK);
    if (s_shuffle_enabled) {
        fill_rect(208, 87, 6, 1, C_GRAY_L);
        draw_triangle(214, 86, 214, 89, 216, 87, C_GRAY_L);
        fill_rect(208, 90, 6, 1, C_GRAY_L);
        draw_triangle(208, 89, 208, 92, 206, 90, C_GRAY_L);
    }

    fill_rect(206, UI_VOL_Y - 5, 107, 12, C_BG);
    fill_rect(229, UI_VOL_Y, 80, 4, C_YELLOW);
    {
        uint16_t vol_x = (uint16_t)(229 + ((s_volume * 67U) / 100U));
        fill_rect(vol_x, UI_VOL_Y - 3, 13, 10, C_GRAY_L);
        fill_rect((uint16_t)(vol_x + 2), UI_VOL_Y - 1, 9, 6, C_GRAY_D);
    }

    fill_rect(229, UI_LIG_Y - 5, 44, 12, C_BG);
    fill_rect(229, UI_LIG_Y, 40, 4, C_MAGENTA);
    fill_rect((uint16_t)(229 + s_brightness * 7), UI_LIG_Y - 3, 13, 10, C_GRAY_L);
    fill_rect((uint16_t)(231 + s_brightness * 7), UI_LIG_Y - 1, 9, 6, C_GRAY_D);

    draw_text5x7(200, 143, "VOL", C_GRAY_L, C_BG);
    draw_text5x7(200, 218, "LIG", C_GRAY_L, C_BG);

    fill_rect(282, 220, 28, 8, C_BG);
    {
        char bp[5];
        uint8_t p = s_battery_percent;
        bp[0] = (p >= 100) ? '1' : ((p >= 10) ? (char)('0' + (p / 10)) : ' ');
        bp[1] = (p >= 100) ? '0' : ((p >= 10) ? (char)('0' + (p % 10)) : (char)('0' + p));
        bp[2] = (p >= 100) ? '0' : '%';
        bp[3] = (p >= 100) ? '%' : 0;
        bp[4] = 0;
        draw_text5x7(282, 220, bp, C_GREEN, C_BG);
    }

    if (s_pressed_button != s_last_pressed_button || s_is_playing != s_last_playing_icon || s_shuffle_enabled != s_last_shuffle_enabled) {
        draw_transport_buttons();
        s_last_pressed_button = s_pressed_button;
        s_last_playing_icon = s_is_playing;
        s_last_shuffle_enabled = s_shuffle_enabled;
    }
}

void GuiManager_SetTrackName(const char *name) { if (name == NULL || name[0] == 0) strncpy(s_track_name, "No track", 64); else strncpy(s_track_name, name, 64); s_track_name[63] = 0; }
void GuiManager_SetCurrentTrackByName(const char *name)
{
    if (name == NULL || name[0] == 0) return;
    for (uint16_t i = 0; i < s_track_count; i++) {
        if (s_tracks[i] != NULL && strcmp(s_tracks[i], name) == 0) {
            s_selected = (int)i;
            ensure_selected_visible();
            s_list_dirty = 1U;
            return;
        }
    }
}
void GuiManager_SetPlaying(uint8_t playing) { s_is_playing = playing; }
void GuiManager_SetShuffle(uint8_t enabled) { s_shuffle_enabled = enabled ? 1U : 0U; }
void GuiManager_SetPlaybackTime(uint16_t mm, uint16_t ss) { s_time_mm = mm; s_time_ss = ss; }
void GuiManager_SetSpectrumLevels(const uint8_t *levels, uint8_t count)
{
    if (levels == NULL) return;
    if (count > 14U) count = 14U;
    for (uint8_t i = 0; i < count; i++) {
        uint8_t h = levels[i];
        if (h > 8U) h = 8U;
        s_viz_heights[i] = h;
    }
}

void GuiManager_RenderSpectrumOnly(void)
{
    draw_spectrum_bars(1U);
}

void GuiManager_HandleTouch(uint16_t x, uint16_t y, uint8_t pressed)
{
    if (!pressed) {
        s_prev_pressed = 0;
        s_pressed_button = -1;
        s_slider_mode = 0;
        s_prev_touch_y = -1;
        return;
    }

    if (x >= UI_LIST_X && x < (UI_LIST_X + UI_LIST_W) && y >= UI_LIST_Y && y < (UI_LIST_Y + UI_LIST_H) && s_track_count > 0) {
        if (s_prev_touch_y >= 0) {
            int16_t dy = (int16_t)y - s_prev_touch_y;
            if (dy >= 10 || dy <= -10) {
                int16_t rows = dy / (int16_t)UI_LIST_ROW_H;
                if (rows == 0) rows = (dy > 0) ? 1 : -1;
                int32_t new_top = (int32_t)s_list_top - rows;
                if (new_top < 0) new_top = 0;
                uint16_t max_top = list_max_top();
                if (new_top > (int32_t)max_top) new_top = max_top;
                if ((uint16_t)new_top != s_list_top) {
                    s_list_top = (uint16_t)new_top;
                    s_list_dirty = 1U;
                }
                s_prev_touch_y = (int16_t)y;
                return;
            }
        }
        s_prev_touch_y = (int16_t)y;
    }

    if (!(x >= UI_LIST_X && x < (UI_LIST_X + UI_LIST_W) && y >= UI_LIST_Y && y < (UI_LIST_Y + UI_LIST_H))) {
        s_prev_touch_y = -1;
    }

    if (x >= UI_SCROLL_X && x < (UI_SCROLL_X + UI_SCROLL_W) && y >= UI_LIST_Y && y < (UI_LIST_Y + UI_LIST_H) && s_track_count > 0) {
        uint16_t max_top = list_max_top();
        if (max_top > 0) {
            uint16_t pos = (uint16_t)(y - UI_LIST_Y);
            if (pos > (UI_LIST_H - 1U)) pos = (UI_LIST_H - 1U);
            s_list_top = (uint16_t)((uint32_t)pos * max_top / (UI_LIST_H - 1U));
            s_list_dirty = 1U;
        }
        return;
    }

    if (x >= UI_LIST_X && x < (UI_LIST_X + UI_LIST_W) && y >= UI_LIST_Y && y < (UI_LIST_Y + UI_LIST_H) && s_track_count > 0 && s_prev_touch_y >= 0) {
        if (s_prev_pressed) {
            return;
        }
        uint16_t row = touch_to_row(y);
        uint16_t idx = (uint16_t)(s_list_top + row);
        if (idx < s_track_count) {
            s_selected = (int)idx;
            s_list_dirty = 1U;
            GuiManager_OnTrackSelect(s_tracks[idx]);
        }
        s_prev_pressed = 1;
        return;
    }

    if (y >= UI_BTN_Y && y < (UI_BTN_Y + UI_BTN_H)) {
        s_slider_mode = 0;
        uint16_t b0 = UI_BTN0_X;
        uint16_t b1 = (uint16_t)(UI_BTN0_X + UI_BTN_GAP);
        uint16_t b2 = (uint16_t)(UI_BTN0_X + 2 * UI_BTN_GAP);
        uint16_t b3 = (uint16_t)(UI_BTN0_X + 3 * UI_BTN_GAP);

        if (x >= b0 && x < (b0 + UI_BTN_W)) s_pressed_button = 0;
        else if (x >= b1 && x < (b1 + UI_BTN_W)) s_pressed_button = 1;
        else if (x >= b2 && x < (b2 + UI_BTN_W)) s_pressed_button = 2;
        else if (x >= b3 && x < (b3 + UI_BTN_W)) s_pressed_button = 3;
        else s_pressed_button = -1;

        if (s_prev_pressed) {
            return;
        }
        s_prev_pressed = 1;

        if (s_pressed_button == 0) { if (s_is_playing) GuiManager_OnPause(); else GuiManager_OnPlay(); }
        else if (s_pressed_button == 1) GuiManager_OnPrev();
        else if (s_pressed_button == 2) GuiManager_OnNext();
        else if (s_pressed_button == 3) GuiManager_OnShuffleToggle();
        return;
    }

    s_prev_touch_y = -1;

    if (!pressed) {
        return;
    }

    if (s_prev_pressed) {
        return;
    }

    s_prev_pressed = 1;

    if (x >= 72 && x < 111 && y >= 2 && y < 14) {
        GuiManager_OnRefreshPlaylist();
        return;
    }

    if (x >= 229 && x < 309 && y >= (UI_VOL_Y - 6) && y < (UI_VOL_Y + 8)) {
        s_slider_mode = 1;
        s_pressed_button = -1;
        int nx = (int)x - 229;
        if (nx < 0) nx = 0;
        if (nx > 67) nx = 67;
        uint16_t v = (uint16_t)(((nx * 100) + 33) / 67);
        v = (uint16_t)(((v + 5) / 10) * 10);
        if (v > 100) v = 100;
        if (v != s_volume) {
            s_volume = v;
            GuiManager_OnVolumeChange(s_volume);
        }
        return;
    }

    if (x >= 229 && x < 269 && y >= (UI_LIG_Y - 6) && y < (UI_LIG_Y + 8)) {
        s_slider_mode = 2;
        s_pressed_button = -1;
        int nx = (int)x - 229;
        if (nx < 0) nx = 0;
        if (nx > 34) nx = 34;
        uint8_t b = (uint8_t)((nx + 3) / 7);
        if (b > 4) b = 4;
        if (b != s_brightness) {
            s_brightness = b;
            GuiManager_OnBrightnessChange(s_brightness);
        }
        return;
    }

    s_slider_mode = 0;
    s_pressed_button = -1;
    return;
}

__attribute__((weak)) void GuiManager_OnPlay(void) {}
__attribute__((weak)) void GuiManager_OnPause(void) {}
__attribute__((weak)) void GuiManager_OnStop(void) {}
__attribute__((weak)) void GuiManager_OnShuffleToggle(void) {}
__attribute__((weak)) void GuiManager_OnNext(void) {}
__attribute__((weak)) void GuiManager_OnPrev(void) {}
__attribute__((weak)) void GuiManager_OnTrackSelect(const char *name) { (void)name; }
