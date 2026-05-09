#include "simple_gfx.h"
#include "simple_gfx_draw.h"
#include "lcd_spi_port.h"
#include "main.h"
#include <string.h>
#include <stdlib.h>

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
static char s_track_name[64] = "No track";
static char s_last_track_name[64] = "";
static uint8_t s_is_playing = 0U;
static uint8_t s_last_playing = 0xFF;
static const char *s_tracks[64];
static uint16_t s_track_count = 0;
static int s_selected = -1;
static uint8_t s_prev_pressed = 0;
static uint8_t s_viz_heights[14] = {0};
static uint8_t s_viz_smoothed[14] = {0};
static uint8_t s_viz_prev_heights[14] = {0};
static uint16_t s_volume = 10; /* percent, 0..100 step 10 */
static uint8_t s_brightness = 4;
static uint8_t s_slider_mode = 0; /* 0 none, 1 vol, 2 lig */

static SimpleGFX_Callbacks_t s_cb = {0};

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
static uint16_t y_to_row(uint16_t y);
static void ensure_selected_visible(void);
static void draw_spectrum_bars(uint8_t decay_step);

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

        GFX_FillRect(x, y, UI_BTN_W, UI_BTN_H, face);
        if (s_pressed_button == i) {
            GFX_FillRect(x, y, UI_BTN_W, 1, lo);
            GFX_FillRect(x, y, 1, UI_BTN_H, lo);
            GFX_FillRect(x, (uint16_t)(y + UI_BTN_H - 1), UI_BTN_W, 1, hi);
            GFX_FillRect((uint16_t)(x + UI_BTN_W - 1), y, 1, UI_BTN_H, hi);
        } else {
            GFX_FillRect(x, y, UI_BTN_W, 1, hi);
            GFX_FillRect(x, y, 1, UI_BTN_H, hi);
            GFX_FillRect(x, (uint16_t)(y + UI_BTN_H - 1), UI_BTN_W, 1, lo);
            GFX_FillRect((uint16_t)(x + UI_BTN_W - 1), y, 1, UI_BTN_H, lo);
        }
    }

    GFX_DrawText5x7(205, 171, "A", C_BLACK, C_GRAY_D);
    GFX_DrawText5x7(234, 171, "P", C_BLACK, C_GRAY_D);
    GFX_DrawText5x7(263, 171, "N", C_BLACK, C_GRAY_D);
    GFX_DrawText5x7(292, 171, "B", C_BLACK, C_GRAY_D);

    int oA = (s_pressed_button == 0) ? 1 : 0;
    int oP = (s_pressed_button == 1) ? 1 : 0;
    int oN = (s_pressed_button == 2) ? 1 : 0;
    int oB = (s_pressed_button == 3) ? 1 : 0;

    if (s_is_playing) GFX_FillRect((uint16_t)(204 + oA), (uint16_t)(181 + oA), 8, 10, C_GRAY_L);
    else { GFX_FillRect((uint16_t)(204 + oA), (uint16_t)(181 + oA), 8, 8, C_GRAY_D); GFX_DrawTriangle((uint16_t)(204 + oA), (uint16_t)(181 + oA), (uint16_t)(204 + oA), (uint16_t)(191 + oA), (uint16_t)(211 + oA), (uint16_t)(186 + oA), C_GRAY_L); }

    GFX_DrawTriangle((uint16_t)(229 + oP), (uint16_t)(186 + oP), (uint16_t)(236 + oP), (uint16_t)(181 + oP), (uint16_t)(236 + oP), (uint16_t)(191 + oP), C_GRAY_L);
    GFX_DrawTriangle((uint16_t)(236 + oP), (uint16_t)(186 + oP), (uint16_t)(243 + oP), (uint16_t)(181 + oP), (uint16_t)(243 + oP), (uint16_t)(191 + oP), C_GRAY_L);

    GFX_DrawTriangle((uint16_t)(262 + oN), (uint16_t)(181 + oN), (uint16_t)(262 + oN), (uint16_t)(191 + oN), (uint16_t)(269 + oN), (uint16_t)(186 + oN), C_GRAY_L);
    GFX_DrawTriangle((uint16_t)(269 + oN), (uint16_t)(181 + oN), (uint16_t)(269 + oN), (uint16_t)(191 + oN), (uint16_t)(276 + oN), (uint16_t)(186 + oN), C_GRAY_L);

    GFX_FillRect((uint16_t)(291 + oB), (uint16_t)(183 + oB), 10, 2, C_GRAY_L);
    GFX_DrawTriangle((uint16_t)(301 + oB), (uint16_t)(181 + oB), (uint16_t)(301 + oB), (uint16_t)(187 + oB), (uint16_t)(305 + oB), (uint16_t)(184 + oB), C_GRAY_L);
    GFX_FillRect((uint16_t)(291 + oB), (uint16_t)(190 + oB), 10, 2, C_GRAY_L);
    GFX_DrawTriangle((uint16_t)(291 + oB), (uint16_t)(188 + oB), (uint16_t)(291 + oB), (uint16_t)(194 + oB), (uint16_t)(287 + oB), (uint16_t)(191 + oB), C_GRAY_L);
}

static void draw_static(void)
{
    GFX_FillRect(0, 0, 320, 240, C_BG);
    GFX_FillRect(UI_LIST_X, UI_LIST_Y, UI_LIST_W, UI_LIST_H, C_BLACK);
    GFX_FillRect(UI_SCROLL_X, UI_LIST_Y, UI_SCROLL_W, UI_LIST_H, 0x0841);
    GFX_FillRect(UI_SCROLL_X + 2, UI_LIST_Y + 4, 1, 24, 0xC618);

    GFX_FillRect(5, 7, 67, 2, C_ORANGE);
    GFX_FillRect(112, 7, 67, 2, C_ORANGE);
    GFX_FillRect(253, 4, 57, 2, C_ORANGE);
    GFX_FillRect(253, 11, 57, 4, C_GRAY_D);

    GFX_FillRect(4, 11, 1, 218, C_GRAY_L);
    GFX_FillRect(182, 11, 1, 218, C_GRAY_L);
    GFX_FillRect(4, 229, 178, 1, C_GRAY_L);
    GFX_FillRect(0, 0, 320, 1, C_GRAY_L);
    GFX_FillRect(0, 239, 320, 1, C_GRAY_L);
    GFX_FillRect(0, 0, 1, 240, C_GRAY_L);
    GFX_FillRect(319, 0, 1, 240, C_GRAY_L);
    GFX_FillRect(184, 0, 1, 240, C_GRAY_L);
    GFX_FillRect(185, 0, 4, 240, C_BLACK);
    GFX_FillRect(190, 0, 1, 240, C_GRAY_L);

    GFX_FillRect(195, 20, UI_STATUS_W, UI_STATUS_H, C_BLACK);
    GFX_FillRect(195, UI_TRACK_Y, UI_TRACK_W, UI_TRACK_H, C_BLACK);
    GFX_FillRect(195, 20, 1, UI_STATUS_H, C_GRAY_L);
    GFX_FillRect(195, 20, UI_STATUS_W, 1, C_GRAY_L);

    draw_transport_buttons();

    GFX_FillRect(229, UI_VOL_Y, 80, 4, C_YELLOW);
    GFX_FillRect(245, UI_VOL_Y - 3, 13, 10, C_GRAY_L);
    GFX_FillRect(247, UI_VOL_Y - 1, 9, 6, C_GRAY_D);

    GFX_FillRect(229, UI_LIG_Y, 40, 4, C_MAGENTA);
    GFX_FillRect(229, UI_LIG_Y - 3, 13, 10, C_GRAY_L);
    GFX_FillRect(231, UI_LIG_Y - 1, 9, 6, C_GRAY_D);

    GFX_FillRect(277, 214, 33, 18, C_BG);
    GFX_FillRect(277, 214, 33, 1, C_GREEN);
    GFX_FillRect(277, 231, 33, 1, C_GREEN);
    GFX_FillRect(277, 214, 1, 18, C_GREEN);
    GFX_FillRect(309, 214, 1, 18, C_GREEN);
    GFX_FillRect(310, 219, 4, 8, C_GREEN);

    GFX_DrawText5x7Scale3_2(75, 2, "LIST", C_GRAY_L, C_BG);
    GFX_DrawText5x7Scale3_2(196, 4, "WINAMP", C_GRAY_L, C_BG);
    GFX_DrawText5x7(200, 143, "VOL", C_GRAY_L, C_BG);
    GFX_DrawText5x7(200, 218, "LIG", C_GRAY_L, C_BG);

    GFX_DrawDseg16Text(232, 48, "00:00", C_GREEN, C_BLACK);
    GFX_DrawText5x7(282, 219, "100%", C_GREEN, C_BG);
}

void SimpleGFX_Init(void) { draw_static(); s_list_dirty = 1U; }
void SimpleGFX_ClearPlaylist(void) { s_track_count = 0; s_selected = -1; s_list_top = 0; draw_static(); memset(s_viz_prev_heights, 0, sizeof(s_viz_prev_heights)); s_list_dirty = 1U; }
void SimpleGFX_AddTrackToList(const char *name) { if (s_track_count < 64) { s_tracks[s_track_count++] = name; s_list_dirty = 1U; } }

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

static uint16_t y_to_row(uint16_t y)
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

    GFX_FillRect(UI_SCROLL_X, UI_LIST_Y, UI_SCROLL_W, UI_LIST_H, 0x0841);
    GFX_FillRect(UI_SCROLL_X, UI_LIST_Y + s_scroll_pos, UI_SCROLL_W, UI_SCROLL_THUMB_H, 0xC618);
    GFX_FillRect(UI_SCROLL_X + 2, UI_LIST_Y + s_scroll_pos + 7, 1, UI_SCROLL_THUMB_H - 14, C_GRAY_L);
}

static void draw_playlist_rows(void)
{
    uint16_t vis = list_visible_rows();
    for (uint16_t row = 0; row < vis; row++) {
        uint16_t idx = (uint16_t)(s_list_top + row);
        uint16_t y = (uint16_t)(18 + row * UI_LIST_ROW_H);
        GFX_FillRect(10, y, 166, 8, C_BLACK);
        if (idx < s_track_count && s_tracks[idx] != NULL) {
            uint16_t tc = (s_selected == (int)idx) ? C_WHITE : C_GREEN;
            GFX_DrawText5x7Ellipsis(10, y, s_tracks[idx], tc, C_BLACK, 27U);
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

    GFX_FillRect(UI_VIZ_X, 50, 69, 41, C_BLACK);
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
        for (int j = 0; j < h; j++) GFX_FillRect(x, (uint16_t)(UI_VIZ_BASE_Y - j * 5), 4, 3, C_GRAY_D);
        s_viz_prev_heights[i] = h;
    }
}

void SimpleGFX_Update(void)
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
        GFX_FillRect(205, 28, 26, 48, C_BLACK);
        if (s_is_playing) {
            GFX_DrawTriangle(215, 31, 215, 47, 230, 39, C_GREEN);
            GFX_DrawText5x7Scale3_2(200, 30, "P", C_GRAY_D, C_BLACK);
            GFX_DrawText5x7Scale3_2(200, 43, "L", C_GRAY_D, C_BLACK);
            GFX_DrawText5x7Scale3_2(200, 56, "A", C_GRAY_D, C_BLACK);
            GFX_DrawText5x7Scale3_2(200, 69, "Y", C_GRAY_D, C_BLACK);
            GFX_FillRect(204, 181, 8, 10, C_GRAY_L);
        } else {
            GFX_FillRect(215, 31, 16, 16, C_RED);
            GFX_DrawText5x7Scale3_2(200, 30, "S", C_GRAY_D, C_BLACK);
            GFX_DrawText5x7Scale3_2(200, 43, "T", C_GRAY_D, C_BLACK);
            GFX_DrawText5x7Scale3_2(200, 56, "O", C_GRAY_D, C_BLACK);
            GFX_DrawText5x7Scale3_2(200, 69, "P", C_GRAY_D, C_BLACK);
            GFX_FillRect(204, 181, 8, 8, C_GRAY_D);
            GFX_DrawTriangle(204, 181, 204, 191, 211, 186, C_GRAY_L);
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
        GFX_FillRect(230, 26, 80, 24, C_BLACK);
        GFX_DrawDseg16Text(232, 48, t, C_GREEN, C_BLACK);
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
            GFX_FillRect(198, 106, (uint16_t)(UI_TRACK_W - 4), 14, C_BLACK);
            GFX_DrawText5x7(198, 115, shown, C_GREEN, C_BLACK);
        }
    }

    draw_spectrum_bars(1U);

    GFX_FillRect(206, 85, 14, 8, C_BLACK);
    if (s_shuffle_enabled) {
        GFX_FillRect(208, 87, 6, 1, C_GRAY_L);
        GFX_DrawTriangle(214, 86, 214, 89, 216, 87, C_GRAY_L);
        GFX_FillRect(208, 90, 6, 1, C_GRAY_L);
        GFX_DrawTriangle(208, 89, 208, 92, 206, 90, C_GRAY_L);
    }

    GFX_FillRect(206, UI_VOL_Y - 5, 107, 12, C_BG);
    GFX_FillRect(229, UI_VOL_Y, 80, 4, C_YELLOW);
    {
        uint16_t vol_x = (uint16_t)(229 + ((s_volume * 67U) / 100U));
        GFX_FillRect(vol_x, UI_VOL_Y - 3, 13, 10, C_GRAY_L);
        GFX_FillRect((uint16_t)(vol_x + 2), UI_VOL_Y - 1, 9, 6, C_GRAY_D);
    }

    GFX_FillRect(229, UI_LIG_Y - 5, 44, 12, C_BG);
    GFX_FillRect(229, UI_LIG_Y, 40, 4, C_MAGENTA);
    GFX_FillRect((uint16_t)(229 + s_brightness * 7), UI_LIG_Y - 3, 13, 10, C_GRAY_L);
    GFX_FillRect((uint16_t)(231 + s_brightness * 7), UI_LIG_Y - 1, 9, 6, C_GRAY_D);

    GFX_DrawText5x7(200, 143, "VOL", C_GRAY_L, C_BG);
    GFX_DrawText5x7(200, 218, "LIG", C_GRAY_L, C_BG);

    GFX_FillRect(282, 220, 28, 8, C_BG);
    {
        char bp[5];
        uint8_t p = s_battery_percent;
        bp[0] = (p >= 100) ? '1' : ((p >= 10) ? (char)('0' + (p / 10)) : ' ');
        bp[1] = (p >= 100) ? '0' : ((p >= 10) ? (char)('0' + (p % 10)) : (char)('0' + p));
        bp[2] = (p >= 100) ? '0' : '%';
        bp[3] = (p >= 100) ? '%' : 0;
        bp[4] = 0;
        GFX_DrawText5x7(282, 220, bp, C_GREEN, C_BG);
    }

    if (s_pressed_button != s_last_pressed_button || s_is_playing != s_last_playing_icon || s_shuffle_enabled != s_last_shuffle_enabled) {
        draw_transport_buttons();
        s_last_pressed_button = s_pressed_button;
        s_last_playing_icon = s_is_playing;
        s_last_shuffle_enabled = s_shuffle_enabled;
    }
}

void SimpleGFX_SetTrackName(const char *name) { if (name == NULL || name[0] == 0) strncpy(s_track_name, "No track", 64); else strncpy(s_track_name, name, 64); s_track_name[63] = 0; }
void SimpleGFX_SetCurrentTrackByName(const char *name)
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
void SimpleGFX_SetPlaying(uint8_t playing) { s_is_playing = playing; }
void SimpleGFX_SetShuffle(uint8_t enabled) { s_shuffle_enabled = enabled ? 1U : 0U; }
void SimpleGFX_SetPlaybackTime(uint16_t mm, uint16_t ss) { s_time_mm = mm; s_time_ss = ss; }
void SimpleGFX_SetSpectrumLevels(const uint8_t *levels, uint8_t count)
{
    if (levels == NULL) return;
    if (count > 14U) count = 14U;
    for (uint8_t i = 0; i < count; i++) {
        uint8_t h = levels[i];
        if (h > 8U) h = 8U;
        s_viz_heights[i] = h;
    }
}

void SimpleGFX_RenderSpectrumOnly(void)
{
    draw_spectrum_bars(1U);
}

void SimpleGFX_ProcessTouch(uint16_t x, uint16_t y, uint8_t pressed)
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
        uint16_t row = y_to_row(y);
        uint16_t idx = (uint16_t)(s_list_top + row);
        if (idx < s_track_count) {
            s_selected = (int)idx;
            s_list_dirty = 1U;
            if (s_cb.on_track_select) s_cb.on_track_select(s_tracks[idx]);
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

        if (s_pressed_button == 0) { if (s_is_playing) { if (s_cb.on_pause) s_cb.on_pause(); } else { if (s_cb.on_play) s_cb.on_play(); } }
        else if (s_pressed_button == 1) { if (s_cb.on_prev) s_cb.on_prev(); }
        else if (s_pressed_button == 2) { if (s_cb.on_next) s_cb.on_next(); }
        else if (s_pressed_button == 3) { if (s_cb.on_shuffle_toggle) s_cb.on_shuffle_toggle(); }
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
        if (s_cb.on_refresh_playlist) s_cb.on_refresh_playlist();
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
            if (s_cb.on_volume_change) s_cb.on_volume_change(s_volume);
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
            if (s_cb.on_brightness_change) s_cb.on_brightness_change(s_brightness);
        }
        return;
    }

    s_slider_mode = 0;
    s_pressed_button = -1;
    return;
}

void SimpleGFX_SetCallbacks(const SimpleGFX_Callbacks_t *cb)
{
    if (cb != NULL) {
        s_cb = *cb;
    }
}
