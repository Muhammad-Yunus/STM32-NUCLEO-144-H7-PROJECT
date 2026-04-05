/**
 ******************************************************************************
 * @file    touch.c
 * @brief   XPT2046 resistive touch controller – pure driver, no LCD/GUI deps.
 *          Adapted for STM32 Nucleo-144 H753ZI (HAL-based GPIO).
 *
 * UI / Calibration has been moved to touch_calib_gui.c
 ******************************************************************************
 */

#include "touch.h"
#include "stm32h7xx_hal.h"
#include <stdlib.h>

/* ── Device instance ─────────────────────────────────────────────────────── */
_m_tp_dev tp_dev =
{
    TP_Init,
    TP_Scan,
    0, 0, 0, 0, 0,
    0.0f, 0.0f, 0, 0, 0
};

uint8_t CMD_RDX = XPT2046_CMD_X;
uint8_t CMD_RDY = XPT2046_CMD_Y;

/* ── Small delay helper ──────────────────────────────────────────────────── */
/*
 * For 64 MHz SYSCLK: each loop iteration ≈ 4 cycles (including decrement/branch)
 * Delay = cnt * 4 / 64MHz = cnt * 0.0625µs
 * So for 1µs: cnt = 16
 */
static inline void tp_delay_us(uint32_t us)
{
    volatile uint32_t cnt = us * 16;  /* 16 iterations per µs at 64 MHz */
    while (cnt--) { }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Low-level SPI bit-bang
 * ═══════════════════════════════════════════════════════════════════════════ */

void TP_Write_Byte(uint8_t num)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        if (num & 0x80) TP_DIN_SET();
        else            TP_DIN_CLR();
        num <<= 1;
        TP_CLK_CLR();
        tp_delay_us(1);
        TP_CLK_SET();
    }
}

uint16_t TP_Read_AD(uint8_t CMD)
{
    uint16_t num = 0;

    TP_CLK_CLR();
    TP_DIN_CLR();
    TP_CS_CLR();

    TP_Write_Byte(CMD);
    tp_delay_us(6);     /* XPT2046 conversion time ≈ 6 µs */

    TP_CLK_CLR();
    tp_delay_us(1);
    TP_CLK_SET();       /* rising edge clocks out BUSY */
    tp_delay_us(1);
    TP_CLK_CLR();

    for (uint8_t i = 0; i < 16; i++)
    {
        num <<= 1;
        TP_CLK_CLR();
        tp_delay_us(1);
        TP_CLK_SET();
        if (TP_DOUT_READ()) num++;
    }

    TP_CS_SET();
    num >>= 4;          /* only top 12 bits are valid */
    return num;
}

/* ── Median-filtered read ────────────────────────────────────────────────── */
#define READ_TIMES  5
#define LOST_VAL    1

uint16_t TP_Read_XOY(uint8_t xy)
{
    uint16_t buf[READ_TIMES];
    uint32_t sum = 0;

    for (uint8_t i = 0; i < READ_TIMES; i++)
        buf[i] = TP_Read_AD(xy);

    /* Bubble sort */
    for (uint8_t i = 0; i < READ_TIMES - 1; i++)
        for (uint8_t j = i + 1; j < READ_TIMES; j++)
            if (buf[i] > buf[j])
            {
                uint16_t tmp = buf[i];
                buf[i] = buf[j];
                buf[j] = tmp;
            }

    for (uint8_t i = LOST_VAL; i < READ_TIMES - LOST_VAL; i++)
        sum += buf[i];

    return (uint16_t)(sum / (READ_TIMES - 2 * LOST_VAL));
}

uint8_t TP_Read_XY(uint16_t *x, uint16_t *y)
{
    uint16_t X, Y;
    X = TP_Read_XOY(CMD_RDX);
    Y = TP_Read_XOY(CMD_RDY);

    /* Discard out-of-range readings */
    if (X > 4095 || Y > 4095) return 0;
    *x = X;
    *y = Y;
    return 1;
}

/* Double-sample read for noise immunity */
uint8_t TP_Read_XY2(uint16_t *x, uint16_t *y)
{
    uint16_t x1, y1, x2, y2;

    if (!TP_Read_XY(&x1, &y1)) return 0;
    if (!TP_Read_XY(&x2, &y2)) return 0;

    if ((abs(x1 - x2) > 200) || (abs(y1 - y2) > 200)) return 0;

    *x = (x1 + x2) / 2;
    *y = (y1 + y2) / 2;
    return 1;
}

/* ── Touch scan using PEN pin detection ──────────────────────────────────── */
uint8_t TP_Scan(uint8_t tp)
{
    /* PEN pin is active LOW when touched */
    if (TP_PEN_READ() == GPIO_PIN_RESET)
    {
        /* Touch detected - read coordinates */
        if (tp == 0)    /* screen coordinates */
        {
            uint16_t rx, ry;
            if (TP_Read_XY2(&rx, &ry))
            {
                if (tp_dev.touchtype == 0)  /* portrait / normal */
                {
                    tp_dev.x = (int16_t)(tp_dev.xfac * rx) + tp_dev.xoff;
                    tp_dev.y = (int16_t)(tp_dev.yfac * ry) + tp_dev.yoff;
                }
                else                        /* swapped X/Y */
                {
                    tp_dev.x = (int16_t)(tp_dev.xfac * ry) + tp_dev.xoff;
                    tp_dev.y = (int16_t)(tp_dev.yfac * rx) + tp_dev.yoff;
                }

                tp_dev.sta |= TP_PRES_DOWN | TP_CATH_PRES;
            }
            else
            {
                tp_dev.sta &= ~(TP_PRES_DOWN | TP_CATH_PRES);
            }
        }
        else            /* raw ADC coordinates */
        {
            if (TP_Read_XY2(&tp_dev.x, &tp_dev.y))
                tp_dev.sta |= TP_PRES_DOWN | TP_CATH_PRES;
            else
                tp_dev.sta &= ~(TP_PRES_DOWN | TP_CATH_PRES);
        }
    }
    else
    {
        /* No touch detected */
        if (tp_dev.sta & TP_PRES_DOWN)
            tp_dev.sta &= ~TP_PRES_DOWN;
        else
            tp_dev.sta &= ~(TP_PRES_DOWN | TP_CATH_PRES);
    }
    return tp_dev.sta;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  TP_Init
 * ═══════════════════════════════════════════════════════════════════════════ */
uint8_t TP_Init(void)
{
    GPIO_InitTypeDef GPIO_Initure = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    /* CLK, MOSI, CS → PB3, PB5, PB6 : output */
    GPIO_Initure.Pin   = TP_CLK_PIN | TP_DIN_PIN | TP_CS_PIN;
    GPIO_Initure.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_Initure.Pull  = GPIO_PULLUP;
    GPIO_Initure.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_Initure);

    /* MISO → PB4 : input */
    GPIO_Initure.Pin  = TP_DOUT_PIN;
    GPIO_Initure.Mode = GPIO_MODE_INPUT;
    GPIO_Initure.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(TP_DOUT_PORT, &GPIO_Initure);

    /* PEN → PG9 : input (polling mode) */
    GPIO_Initure.Pin   = TP_PEN_PIN;
    GPIO_Initure.Mode  = GPIO_MODE_INPUT;
    GPIO_Initure.Pull  = GPIO_PULLUP;
    HAL_GPIO_Init(TP_PEN_PORT, &GPIO_Initure);

    /* Default idle state */
    TP_CS_SET();
    TP_CLK_CLR();
    TP_DIN_CLR();

    /* No dummy wake‑up needed – PEN IRQ works after normal init */

    /* Use default calibration – call TP_Adjust() from touch_calib_gui.c */
    tp_dev.xfac     = TP_CAL_XFAC_DEFAULT;
    tp_dev.yfac     = TP_CAL_YFAC_DEFAULT;
    tp_dev.xoff     = TP_CAL_XOFF_DEFAULT;
    tp_dev.yoff     = TP_CAL_YOFF_DEFAULT;
    tp_dev.touchtype = 0;

    return 0;
}

/* ── Set orientation (call after LCD init/direction) ─────────────────────── */
void TP_SetOrientation(uint8_t orientation)
{
    if (orientation == 0 || orientation == 1)
    {
        CMD_RDX = XPT2046_CMD_X;
        CMD_RDY = XPT2046_CMD_Y;
    }
    else
    {
        CMD_RDX = XPT2046_CMD_Y;
        CMD_RDY = XPT2046_CMD_X;
    }
}
