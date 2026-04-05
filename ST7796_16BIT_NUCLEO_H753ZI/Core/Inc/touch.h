/**
 ******************************************************************************
 * @file    touch.h
 * @brief   XPT2046 resistive touch controller driver (bit-bang SPI)
 *
 * PIN MAPPING (STM32 Nucleo-144 H753ZI):
 *   T_PEN  (IRQ)  -->  PG9   (GPIO_INPUT, active LOW,  EXTI optional)
 *   T_MISO (DOUT) -->  PB4   (GPIO_INPUT)
 *   T_MOSI (DIN)  -->  PB5   (GPIO_OUTPUT)
 *   T_CS          -->  PB6   (GPIO_OUTPUT, active LOW)
 *   T_CLK  (SCK)  -->  PB3   (GPIO_OUTPUT)
 *
 * NOTE: These pins are on the Arduino and ST Morpho headers, which are free
 *       on Nucleo-144 H753ZI when the FMC bus uses PD/PE/PF.
 ******************************************************************************
 */

#ifndef __TOUCH_H
#define __TOUCH_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"
#include <stdint.h>

/* ── Bit flags for tp_dev.sta ────────────────────────────────────────────── */
#define TP_PRES_DOWN    0x80U   /* touch currently pressed     */
#define TP_CATH_PRES    0x40U   /* touch pressed this scan     */

/* ── Pressure threshold for touch detection ──────────────────────────────── */
#define TP_PRESS_THRESHOLD  100U   /* Minimum pressure to detect touch */

/* ── Touch GPIO pin assignment ───────────────────────────────────────────── */
/* PEN → PG9 */
#define TP_PEN_PORT     GPIOG
#define TP_PEN_PIN      GPIO_PIN_9

/* MISO → PB4 */
#define TP_DOUT_PORT    GPIOB
#define TP_DOUT_PIN     GPIO_PIN_4

/* MOSI → PB5 */
#define TP_DIN_PORT     GPIOB
#define TP_DIN_PIN      GPIO_PIN_5

/* CS → PB6 */
#define TP_CS_PORT      GPIOB
#define TP_CS_PIN       GPIO_PIN_6

/* CLK → PB3 */
#define TP_CLK_PORT     GPIOB
#define TP_CLK_PIN      GPIO_PIN_3

/* ── GPIO macros ─────────────────────────────────────────────────────────── */
#define TP_PEN_READ()   HAL_GPIO_ReadPin(TP_PEN_PORT, TP_PEN_PIN)
#define TP_DOUT_READ()  HAL_GPIO_ReadPin(TP_DOUT_PORT, TP_DOUT_PIN)

#define TP_DIN_SET()    HAL_GPIO_WritePin(TP_DIN_PORT, TP_DIN_PIN, GPIO_PIN_SET)
#define TP_DIN_CLR()    HAL_GPIO_WritePin(TP_DIN_PORT, TP_DIN_PIN, GPIO_PIN_RESET)
#define TP_CLK_SET()    HAL_GPIO_WritePin(TP_CLK_PORT, TP_CLK_PIN, GPIO_PIN_SET)
#define TP_CLK_CLR()    HAL_GPIO_WritePin(TP_CLK_PORT, TP_CLK_PIN, GPIO_PIN_RESET)
#define TP_CS_SET()     HAL_GPIO_WritePin(TP_CS_PORT, TP_CS_PIN, GPIO_PIN_SET)
#define TP_CS_CLR()     HAL_GPIO_WritePin(TP_CS_PORT, TP_CS_PIN, GPIO_PIN_RESET)

/* ── XPT2046 commands ────────────────────────────────────────────────────── */
#define XPT2046_CMD_X       0xD0U   /* Read X position */
#define XPT2046_CMD_Y       0x90U   /* Read Y position */
#define XPT2046_CMD_Z1      0xB0U   /* Read Z1 (pressure) */
#define XPT2046_CMD_Z2      0xC0U   /* Read Z2 (pressure) */
#define XPT2046_CMD_TEMP    0xA0U   /* Read temperature */
#define XPT2046_CMD_VBAT    0xA4U   /* Read battery voltage */
#define XPT2046_CMD_AUX     0xA8U   /* Read aux input */
#define XPT2046_CMD_PWD     0x00U   /* Power down mode */
#define XPT2046_CMD_ALWAYSON  0x03U  /* Always on / no power down */

/* ── Calibration defaults (override after calibration) ─────────────────── */
#define TP_CAL_XFAC_DEFAULT   0.0f
#define TP_CAL_YFAC_DEFAULT   0.0f
#define TP_CAL_XOFF_DEFAULT   0
#define TP_CAL_YOFF_DEFAULT   0

/* ── Device descriptor ───────────────────────────────────────────────────── */
typedef struct
{
    uint8_t (*init)(void);
    uint8_t (*scan)(uint8_t);
    uint16_t x0;          /* raw X on first press          */
    uint16_t y0;          /* raw Y on first press          */
    uint16_t x;           /* calibrated screen X           */
    uint16_t y;           /* calibrated screen Y           */
    uint8_t  sta;         /* status flags (TP_PRES_DOWN…)  */
    float    xfac;
    float    yfac;
    short    xoff;
    short    yoff;
    uint8_t  touchtype;   /* 0=normal, 1=swapped X/Y       */
} _m_tp_dev;

extern _m_tp_dev tp_dev;

/* ── Public API ──────────────────────────────────────────────────────────── */
uint8_t  TP_Init(void);
uint8_t  TP_Scan(uint8_t tp);
void     TP_Write_Byte(uint8_t num);
uint16_t TP_Read_AD(uint8_t CMD);
uint16_t TP_Read_XOY(uint8_t xy);
uint8_t  TP_Read_XY(uint16_t *x, uint16_t *y);
uint8_t  TP_Read_XY2(uint16_t *x, uint16_t *y);
void     TP_SetOrientation(uint8_t orientation);  /* 0-3, matches LCD direction */

#ifdef __cplusplus
}
#endif

#endif /* __TOUCH_H */
