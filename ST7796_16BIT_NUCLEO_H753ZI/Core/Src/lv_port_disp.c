/**
 * @file lv_port_disp.c
 * LVGL display driver for ST7796 16-bit parallel LCD via FMC
 *
 * Simple, reliable implementation - DMA2D handles internal rendering.
 */

#include "lv_port_disp.h"
#include "lcd.h"
#include "main.h"
#include <string.h>

#define LCD_WIDTH   480
#define LCD_HEIGHT  320

/* Display buffers (120 lines each) stored in RAM_D2 */
#define LVGL_BUFFER_SIZE  (LCD_WIDTH * 120)
static lv_color_t buf1[LVGL_BUFFER_SIZE] __attribute__((section(".RAM_D2"), aligned(32)));
static lv_color_t buf2[LVGL_BUFFER_SIZE] __attribute__((section(".RAM_D2"), aligned(32)));

/* Set LCD write window */
static inline void set_write_window(uint16_t x1, uint16_t x2, uint16_t y1, uint16_t y2)
{
    LCD_WR_REG(0x2A);
    LCD->LCD_RAM = (x1 >> 8);
    LCD->LCD_RAM = (x1 & 0xFF);
    LCD->LCD_RAM = (x2 >> 8);
    LCD->LCD_RAM = (x2 & 0xFF);

    LCD_WR_REG(0x2B);
    LCD->LCD_RAM = (y1 >> 8);
    LCD->LCD_RAM = (y1 & 0xFF);
    LCD->LCD_RAM = (y2 >> 8);
    LCD->LCD_RAM = (y2 & 0xFF);

    LCD_WR_REG(0x2C);
}

/* Write pixels to LCD */
static inline void write_pixels_fast(const uint16_t *p, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++)
    {
        LCD->LCD_RAM = *p++;
    }
}

/**
 * Flush display callback - simple full area transfer
 */
void lv_port_disp_flush_cb(lv_display_t *disp_drv, const lv_area_t *area, uint8_t *color_p)
{
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    uint32_t total = w * h;

    set_write_window(area->x1, area->x2, area->y1, area->y2);
    write_pixels_fast((uint16_t *)color_p, total);

    lv_display_flush_ready(disp_drv);
}

/**
 * Initialize LVGL display driver
 */
void lv_port_disp_init(void)
{
    lv_display_t *disp = lv_display_create(LCD_WIDTH, LCD_HEIGHT);

    /* Set up double buffering with partial rendering mode */
    lv_display_set_buffers(disp,
                           buf1,
                           buf2,
                           sizeof(buf1),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    /* Set flush callback */
    lv_display_set_flush_cb(disp, lv_port_disp_flush_cb);
}