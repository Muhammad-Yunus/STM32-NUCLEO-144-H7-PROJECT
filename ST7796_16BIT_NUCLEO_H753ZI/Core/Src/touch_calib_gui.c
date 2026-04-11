/**
 ******************************************************************************
 * @file    touch_calib_gui.c
 * @brief   XPT2046 touch screen calibration UI – uses LCD and GUI helpers.
 *
 * This file provides the UI parts of the original TP_Adjust() implementation,
 * plus drawing helpers that were previously mixed into touch.c.
 * It no longer depends on the low‑level driver (touch.c) – only the public
 * functions in touch.h are used for raw ADC reads.
 ******************************************************************************
 */

#include "touch_calib_gui.h"
#include "touch.h"
#include "lcd.h"
#include "gui.h"
#include <stdlib.h>

/* -------------------------------------------------------------------------- */
/*  Drawing helpers – were previously in touch.c (now UI only)                  */
/* -------------------------------------------------------------------------- */

void TP_Draw_Touch_Point(uint16_t x, uint16_t y, uint16_t color)
{
    POINT_COLOR = color;
    GUI_DrawLine(x - 12, y, x + 13, y, color);
    GUI_DrawLine(x, y - 12, x, y + 13, color);
    LCD_DrawPoint(x + 1, y + 1);
    LCD_DrawPoint(x - 1, y + 1);
    LCD_DrawPoint(x + 1, y - 1);
    LCD_DrawPoint(x - 1, y - 1);
    GUI_DrawCircle(x, y, 6, color);
}

void TP_Draw_Big_Point(uint16_t x, uint16_t y, uint16_t color)
{
    POINT_COLOR = color;
    LCD_DrawPoint(x,     y);
    LCD_DrawPoint(x + 1, y);
    LCD_DrawPoint(x,     y + 1);
    LCD_DrawPoint(x + 1, y + 1);
}

/* -------------------------------------------------------------------------- */
/*  Calibration routine – UI only (calls low‑level TP_Read_XY2)                */
/* -------------------------------------------------------------------------- */

void TP_Adjust(void)
{
    uint16_t pos_temp[4][2];   /* raw ADC {x,y} for 4 corner points */
    uint8_t  cnt = 0;
    uint16_t d1, d2;
    uint32_t tem1, tem2;
    float    fac;
    uint16_t outtime = 0;

    POINT_COLOR = RED;
    LCD_Clear(WHITE);
    POINT_COLOR = BLACK;
    BACK_COLOR  = WHITE;

    /* Show instruction */
    GUI_ShowString(lcddev.width / 2 - 90, lcddev.height / 2 - 20,
                   (uint8_t *)"Touch calibration", RED);
    GUI_ShowString(lcddev.width / 2 - 95, lcddev.height / 2,
                   (uint8_t *)"Tap each cross mark", RED);

    TP_Draw_Touch_Point(CAL_MARGIN, CAL_MARGIN, RED);   /* top‑left */

    tp_dev.sta = 0;
    cnt = 0;

    while (1)
    {
        if ((tp_dev.sta & TP_PRES_DOWN) && !(tp_dev.sta & TP_CATH_PRES))
        {
            /* waiting for release */
        }
        TP_Scan(1);   /* raw mode */
        if ((tp_dev.sta & TP_CATH_PRES) && !(tp_dev.sta & TP_PRES_DOWN))
        {
            pos_temp[cnt][0] = tp_dev.x;
            pos_temp[cnt][1] = tp_dev.y;
            cnt++;
            switch (cnt)
            {
                case 1:
                    TP_Draw_Touch_Point(CAL_MARGIN, CAL_MARGIN, WHITE);
                    TP_Draw_Touch_Point(lcddev.width - CAL_MARGIN, CAL_MARGIN, RED);
                    break;
                case 2:
                    TP_Draw_Touch_Point(lcddev.width - CAL_MARGIN, CAL_MARGIN, WHITE);
                    TP_Draw_Touch_Point(CAL_MARGIN, lcddev.height - CAL_MARGIN, RED);
                    break;
                case 3:
                    TP_Draw_Touch_Point(CAL_MARGIN, lcddev.height - CAL_MARGIN, WHITE);
                    TP_Draw_Touch_Point(lcddev.width - CAL_MARGIN, lcddev.height - CAL_MARGIN, RED);
                    break;
                case 4:
                    /* Calculate calibration factors using two-point form */
                    uint32_t raw_x_left  = (pos_temp[0][0] + pos_temp[2][0]) / 2;
                    uint32_t raw_x_right = (pos_temp[1][0] + pos_temp[3][0]) / 2;
                    uint32_t raw_y_top   = (pos_temp[0][1] + pos_temp[1][1]) / 2;
                    uint32_t raw_y_bottom= (pos_temp[2][1] + pos_temp[3][1]) / 2;

                    float target_x_span = (float)(lcddev.width  - 2 * CAL_MARGIN);
                    float target_y_span = (float)(lcddev.height - 2 * CAL_MARGIN);

                    float raw_x_span = (float)((int32_t)raw_x_right - (int32_t)raw_x_left);
                    float raw_y_span = (float)((int32_t)raw_y_bottom - (int32_t)raw_y_top);

                    if (fabsf(raw_x_span) < 0.01f || fabsf(raw_y_span) < 0.01f)
                    {
                        cnt = 0;
                        LCD_Clear(WHITE);
                        TP_Draw_Touch_Point(CAL_MARGIN, CAL_MARGIN, RED);
                        GUI_ShowString(lcddev.width / 2 - 90, lcddev.height / 2 - 20,
                                       (uint8_t *)"Calibration failed!", RED);
                        GUI_ShowString(lcddev.width / 2 - 90, lcddev.height / 2,
                                       (uint8_t *)"Retry...", RED);
                        HAL_Delay(1000);
                        LCD_Clear(WHITE);
                        TP_Draw_Touch_Point(CAL_MARGIN, CAL_MARGIN, RED);
                        break;
                    }

                    tp_dev.xfac = target_x_span / raw_x_span;
                    tp_dev.yfac = target_y_span / raw_y_span;

                    tp_dev.xoff = (short)(CAL_MARGIN - tp_dev.xfac * raw_x_left);
                    tp_dev.yoff = (short)(CAL_MARGIN - tp_dev.yfac * raw_y_top);

                    /* Verify diagonal consistency */

                    /* Verify diagonal consistency */
                    d1 = abs(pos_temp[0][0] - pos_temp[3][0]);
                    d2 = abs(pos_temp[0][1] - pos_temp[3][1]);
                    tem1 = (uint32_t)d1 * d1 + (uint32_t)d2 * d2;
                    d1 = abs(pos_temp[1][0] - pos_temp[2][0]);
                    d2 = abs(pos_temp[1][1] - pos_temp[2][1]);
                    tem2 = (uint32_t)d1 * d1 + (uint32_t)d2 * d2;

                    fac = (float)tem1 / tem2;
                    if (fac < 0.80f || fac > 1.20f ||
                        tp_dev.xfac < 0.01f || fabsf(tp_dev.yfac) < 0.01f)
                    {
                        /* Bad calibration – restart */
                        cnt = 0;
                        LCD_Clear(WHITE);
                        TP_Draw_Touch_Point(CAL_MARGIN, CAL_MARGIN, RED);
                        GUI_ShowString(lcddev.width / 2 - 90, lcddev.height / 2 - 20,
                                       (uint8_t *)"Calibration failed!", RED);
                        GUI_ShowString(lcddev.width / 2 - 90, lcddev.height / 2,
                                       (uint8_t *)"Retry...", RED);
                        HAL_Delay(1000);
                        LCD_Clear(WHITE);
                        TP_Draw_Touch_Point(CAL_MARGIN, CAL_MARGIN, RED);
                    }
                    else
                    {
                        GUI_ShowString(lcddev.width / 2 - 70, lcddev.height / 2,
                                       (uint8_t *)"Calibration OK!", BLUE);
                        GUI_ShowString(lcddev.width / 2 - 70, lcddev.height / 2 + 20,
                                       (uint8_t *)"xfac : ", BLUE);
                        GUI_ShowFloatNum(lcddev.width / 2, lcddev.height / 2 + 20,
                                         tp_dev.xfac, 3, BLUE);
                        GUI_ShowString(lcddev.width / 2 - 70, lcddev.height / 2 + 40,
                                       (uint8_t *)"yfac : ", BLUE);
                        GUI_ShowFloatNum(lcddev.width / 2, lcddev.height / 2 + 40,
                                         tp_dev.yfac, 3, BLUE);
                        GUI_ShowString(lcddev.width / 2 - 70, lcddev.height / 2 + 60,
                                       (uint8_t *)"xoff : ", BLUE);
                        GUI_ShowIntNum(lcddev.width / 2, lcddev.height / 2 + 60,
                                       (int32_t)tp_dev.xoff, 6, BLUE);
                        GUI_ShowString(lcddev.width / 2 - 70, lcddev.height / 2 + 80,
                                       (uint8_t *)"yoff : ", BLUE);
                        GUI_ShowIntNum(lcddev.width / 2, lcddev.height / 2 + 80,
                                       (int32_t)tp_dev.yoff, 6, BLUE);
                        HAL_Delay(10000);
                        LCD_Clear(WHITE);
                        return;
                    }
                    break;
            }
            tp_dev.sta = 0;
            outtime = 0;
        }
        HAL_Delay(10);
        outtime++;
        if (outtime > 1000)   /* 10 s timeout → restart */
        {
            outtime = 0;
            cnt = 0;
            LCD_Clear(WHITE);
            TP_Draw_Touch_Point(CAL_MARGIN, CAL_MARGIN, RED);
        }
    }
}
