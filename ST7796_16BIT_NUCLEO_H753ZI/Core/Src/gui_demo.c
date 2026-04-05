/**
 ******************************************************************************
 * @file    gui_demo.c
 * @brief   Demo functions showcasing the GUI capabilities.
 *
 * Demo suite includes:
 *   - simple_color_loop_test() – basic RGB color cycle
 *   - primitives_demo() – lines, rectangles, circles
 *   - text_demo() – font rendering with cursor and sizing
 *   - animation_demo() – simple bouncing ball animation
 *   - touch_calib_demo() – 4-point touch calibration
 ******************************************************************************
 */

#include "gui_demo.h"
#include "lcd.h"
#include "gui.h"
#include "touch.h"
#include "touch_calib_gui.h"
#include "stm32h7xx_hal.h"

/* ═══════════════════════════════════════════════════════════════════════════
 *  Simple Color Loop Test (moved from main.c)
 * ═══════════════════════════════════════════════════════════════════════════ */

void simple_color_loop_test(void)
{
    LCD_Clear(BLUE);
    HAL_Delay(1000);
    LCD_Clear(RED);
    HAL_Delay(1000);
    LCD_Clear(GREEN);
    HAL_Delay(1000);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Primitives Demo – lines, rectangles, circles
 * ═══════════════════════════════════════════════════════════════════════════ */

void primitives_demo(void)
{
    uint16_t w = lcddev.width;
    uint16_t h = lcddev.height;

    LCD_Clear(WHITE);

    /* Draw grid of horizontal lines */
    for (uint16_t y = 0; y < h; y += 20)
        GUI_DrawFastHLine(0, y, w, BLUE);

    /* Draw grid of vertical lines */
    for (uint16_t x = 0; x < w; x += 20)
        GUI_DrawFastVLine(x, 0, h, BLUE);

    HAL_Delay(500);

    /* Draw rectangles */
    GUI_DrawRect(10, 10, 100, 80, RED);
    GUI_DrawRect(120, 10, 100, 80, GREEN);
    GUI_DrawRect(230, 10, 80, 80, BLUE);

    HAL_Delay(500);

    /* Draw filled rectangles */
    GUI_FillRect(10, 100, 50, 50, RED);
    GUI_FillRect(70, 100, 50, 50, GREEN);
    GUI_FillRect(130, 100, 50, 50, BLUE);

    HAL_Delay(500);

    /* Draw circles */
    GUI_DrawCircle(50, 200, 30, RED);
    GUI_DrawCircle(120, 200, 30, GREEN);
    GUI_DrawCircle(190, 200, 30, BLUE);

    HAL_Delay(500);

    /* Draw filled circles */
    GUI_FillCircle(50, 260, 25, RED);
    GUI_FillCircle(120, 260, 25, GREEN);
    GUI_FillCircle(190, 260, 25, BLUE);

    HAL_Delay(500);

    /* Draw rounded rectangles */
    GUI_DrawRoundRect(10, 300, 100, 50, 10, MAGENTA);
    GUI_FillRoundRect(120, 300, 100, 50, 10, CYAN);

    HAL_Delay(500);

    /* Draw triangles */
    GUI_DrawTriangle(50, 360, 100, 360, 75, 400, YELLOW);
    GUI_FillTriangle(150, 360, 200, 360, 175, 400, YELLOW);

    HAL_Delay(1000);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Text Demo – font rendering with cursor and sizing
 * ═══════════════════════════════════════════════════════════════════════════ */

void text_demo(void)
{
    LCD_Clear(WHITE);

    /* Demo 1: Basic string rendering */
    GUI_SetTextColor(BLACK, WHITE);
    GUI_SetTextSize(1);
    GUI_SetCursor(10, 10);
    GUI_Print("Hello, STM32!");

    HAL_Delay(500);

    /* Demo 2: Multiple lines with automatic cursor wrap */
    GUI_SetCursor(10, 40);
    GUI_Print("Line 1");
    GUI_Println("Line 2");
    GUI_Println("Line 3");

    HAL_Delay(500);

    /* Demo 3: Numbers */
    GUI_SetCursor(10, 100);
    GUI_PrintNum(12345);
    GUI_SetCursor(10, 120);
    GUI_PrintNum(-9876);

    HAL_Delay(500);

    /* Demo 4: Float numbers */
    GUI_SetCursor(10, 150);
    GUI_PrintFloat(3.14159f, 2);
    GUI_SetCursor(10, 170);
    GUI_PrintFloat(2.71828f, 4);

    HAL_Delay(500);

    /* Demo 5: Different text sizes */
    GUI_SetCursor(10, 210);
    GUI_SetTextSize(1);
    GUI_Print("Size 1");
    GUI_SetCursor(80, 210);
    GUI_SetTextSize(2);
    GUI_Print("Size 2");

    HAL_Delay(500);

    /* Demo 6: Colored text */
    GUI_SetCursor(10, 250);
    GUI_SetTextColor(RED, WHITE);
    GUI_Print("Red Text");

    GUI_SetCursor(10, 290);
    GUI_SetTextColor(BLUE, WHITE);
    GUI_Print("Blue Text");

    GUI_SetCursor(10, 310);
    GUI_SetTextColor(GREEN, WHITE);
    GUI_Print("Green Text");

    HAL_Delay(1000);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Touch Calibration Demo
 * ═══════════════════════════════════════════════════════════════════════════ */

void touch_calib_demo(void)
{
    /* Show instruction screen */
    LCD_Clear(WHITE);
    GUI_SetTextColor(BLACK, WHITE);
    GUI_SetTextSize(1);
    GUI_SetCursor(20, 100);
    GUI_Println("Touch Calibration Demo");
    GUI_SetCursor(20, 130);
    GUI_Println("Tap each cross mark");
    GUI_SetCursor(20, 160);
    GUI_Println("to calibrate touch");
    HAL_Delay(2000);

    /* Run calibration */
    TP_Adjust();

    /* Show completion */
    LCD_Clear(WHITE);
    GUI_SetTextColor(GREEN, WHITE);
    GUI_SetCursor(60, lcddev.height / 2);
    GUI_Println("Calibration Done!");
    HAL_Delay(1000);

    /* Touch test - draw where touched */
    LCD_Clear(BLACK);
    GUI_SetTextColor(WHITE, BLACK);
    GUI_SetCursor(10, 10);
    GUI_Println("Touch test - draw on screen");
    GUI_Println("Touch to draw, timeout 10s");

    uint32_t timeout = HAL_GetTick() + 30000; /* 10 second demo */
    while (HAL_GetTick() < timeout)
    {
        if (TP_Scan(0) & TP_PRES_DOWN)
        {
            /* Clamp to screen bounds before drawing */
            int16_t x = tp_dev.x;
            int16_t y = tp_dev.y;
            if (x < 0) x = 0;
            if (y < 0) y = 0;
            if (x >= lcddev.width)  x = lcddev.width - 1;
            if (y >= lcddev.height) y = lcddev.height - 1;

            GUI_FillCircle(x, y, 3, YELLOW);
        }
        HAL_Delay(10);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Animation Demo – bouncing ball
 * ═══════════════════════════════════════════════════════════════════════════ */

void animation_demo(void)
{
    uint16_t w = lcddev.width;
    uint16_t h = lcddev.height;
    int16_t x = w / 2;
    int16_t y = h / 2;
    int16_t dx = 2;
    int16_t dy = 2;
    uint8_t radius = 15;

    LCD_Clear(BLACK);

    for (uint32_t frame = 0; frame < 300; frame++)
    {
        /* Clear previous position */
        LCD_Clear(BLACK);

        /* Draw bouncing ball */
        GUI_FillCircle(x, y, radius, CYAN);
        GUI_DrawCircle(x, y, radius + 3, WHITE);

        /* Update position */
        x += dx;
        y += dy;

        /* Bounce off walls */
        if (x <= radius || x >= w - radius - 1) dx = -dx;
        if (y <= radius || y >= h - radius - 1) dy = -dy;

        HAL_Delay(16);  /* ~60 FPS */
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Full Demo Suite – run all demos in sequence
 * ═══════════════════════════════════════════════════════════════════════════ */

void gui_demo_all(void)
{
	touch_calib_demo();
    simple_color_loop_test();
    primitives_demo();
    text_demo();
    animation_demo();
}
