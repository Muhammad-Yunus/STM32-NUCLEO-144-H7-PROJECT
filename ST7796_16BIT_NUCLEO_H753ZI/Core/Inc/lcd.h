/**
 ******************************************************************************
 * @file    lcd.h
 * @brief   ST7796 3.5" 16-bit parallel LCD driver for STM32 Nucleo-144 H753ZI
 *
 * PIN MAPPING (16-bit parallel via FMC/FSMC - Bank1 NE1, Mode A):
 * ================================================================
 * LCD Module   -->  Nucleo H753ZI Pin  (FMC Signal)
 * ----------------------------------------------------------------
 * DATA BUS (16-bit):
 *   DB0        -->  PD14               (FMC_D0)
 *   DB1        -->  PD15               (FMC_D1)
 *   DB2        -->  PD0                (FMC_D2)
 *   DB3        -->  PD1                (FMC_D3)
 *   DB4        -->  PE7                (FMC_D4)
 *   DB5        -->  PE8                (FMC_D5)
 *   DB6        -->  PE9                (FMC_D6)
 *   DB7        -->  PE10               (FMC_D7)
 *   DB8        -->  PE11               (FMC_D8)
 *   DB9        -->  PE12               (FMC_D9)
 *   DB10       -->  PE13               (FMC_D10)
 *   DB11       -->  PE14               (FMC_D11)
 *   DB12       -->  PE15               (FMC_D12)
 *   DB13       -->  PD8                (FMC_D13)
 *   DB14       -->  PD9                (FMC_D14)
 *   DB15       -->  PD10               (FMC_D15)
 * CONTROL:
 *   WR         -->  PD5                (FMC_NWE)
 *   RD         -->  PD4                (FMC_NOE)
 *   RS (D/C)   -->  PF0                (FMC_A0)  <-- A0 toggles REG/DATA
 *   CS         -->  PD7                (FMC_NE1)
 *   RST        -->  PA8                (GPIO Output, active LOW)
 *   BL         -->  PA9                (GPIO Output, active HIGH)
 * TOUCH (XPT2046) & SD CARD :
 *   SCK        -->  PA5                (SPI1_SCK)
 *   MISO       -->  PA6                (SPI1_MISO)
 *   MOSI       -->  PA7                (SPI1_MOSI)
 *   T_PEN      -->  PG9                (GPIO Input, active LOW)
 *   T_CS       -->  PB6                (GPIO Output)
 *   SD_CS      -->  PB14               (GPIO Output)
 ******************************************************************************
 */

#ifndef __LCD_H
#define __LCD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"
#include <stdint.h>

/* ── User configuration ─────────────────────────────────────────────────── */
#define USE_HORIZONTAL      3   /* 0=0°, 1=90°, 2=180°, 3=270°  -> 3 = portrait*/
#define LCD_USE8BIT_MODEL   0   /* 0=16-bit bus (default), 1=8-bit bus     */

/* ── LCD physical dimensions (portrait mode) ────────────────────────────── */
#define LCD_W   320
#define LCD_H   480

/* ── FMC / Address mapping ──────────────────────────────────────────────── */
/*
 * Bank1 NE1 base: 0x60000000
 * RS line is wired to FMC_A0.
 * In 16-bit mode the MCU shifts the byte address right by 1,
 * so A0 of the MCU internal address bus maps to HADDR[1].
 * Set bit 1 of the offset to assert A0 on the bus → +2 (0x00000002).
 * REG (RS=0): base address     = 0x60000000
 * DATA (RS=1): base + 2        = 0x60000002
 */
typedef struct __attribute__((packed))
{
    volatile uint16_t LCD_REG;   /* RS = 0 → command/index register */
    volatile uint16_t LCD_RAM;   /* RS = 1 → data register          */
} LCD_TypeDef;

#define LCD_BASE    ((uint32_t)(0x60000000 | 0x00000000))
#define LCD         ((LCD_TypeDef *) LCD_BASE)

/* ── GPIO: Reset and Backlight ──────────────────────────────────────────── */
/* RST → PA8  (active LOW) */
#define LCD_RST_GPIO_PORT   GPIOA
#define LCD_RST_GPIO_PIN    GPIO_PIN_8
/* BL  → PA9  (active HIGH) */
#define LCD_BL_GPIO_PORT    GPIOA
#define LCD_BL_GPIO_PIN     GPIO_PIN_9

#define LCD_RST_LOW()   HAL_GPIO_WritePin(LCD_RST_GPIO_PORT, LCD_RST_GPIO_PIN, GPIO_PIN_RESET)
#define LCD_RST_HIGH()  HAL_GPIO_WritePin(LCD_RST_GPIO_PORT, LCD_RST_GPIO_PIN, GPIO_PIN_SET)
#define LCD_BL_ON()     HAL_GPIO_WritePin(LCD_BL_GPIO_PORT,  LCD_BL_GPIO_PIN,  GPIO_PIN_SET)
#define LCD_BL_OFF()    HAL_GPIO_WritePin(LCD_BL_GPIO_PORT,  LCD_BL_GPIO_PIN,  GPIO_PIN_RESET)

/* ── MPU region for LCD SRAM window ────────────────────────────────────── */
#define LCD_REGION_NUMBER   MPU_REGION_NUMBER0
#define LCD_ADDRESS_START   (0x60000000UL)
#define LCD_REGION_SIZE     MPU_REGION_SIZE_256MB

/* ── Basic colours (RGB565) ─────────────────────────────────────────────── */
#define WHITE       0xFFFF
#define BLACK       0x0000
#define BLUE        0x001F
#define BRED        0xF81F
#define GRED        0xFFE0
#define GBLUE       0x07FF
#define RED         0xF800
#define MAGENTA     0xF81F
#define GREEN       0x07E0
#define CYAN        0x7FFF
#define YELLOW      0xFFE0
#define BROWN       0xBC40
#define BRRED       0xFC07
#define GRAY        0x8430
#define DARKBLUE    0x01CF
#define LIGHTBLUE   0x7D7C
#define GRAYBLUE    0x5458
#define LIGHTGREEN  0x841F
#define LGRAY       0xC618
#define LGRAYBLUE   0xA651
#define LBBLUE      0x2B12

/* ── LCD device descriptor ──────────────────────────────────────────────── */
typedef struct
{
    uint16_t width;
    uint16_t height;
    uint16_t id;
    uint8_t  dir;
    uint16_t wramcmd;
    uint16_t rramcmd;
    uint16_t setxcmd;
    uint16_t setycmd;
} _lcd_dev;

extern _lcd_dev lcddev;
extern uint16_t POINT_COLOR;
extern uint16_t BACK_COLOR;

/* ── Public API ─────────────────────────────────────────────────────────── */
void LCD_Init(void);
void LCD_Clear(uint16_t Color);
void LCD_SetCursor(uint16_t Xpos, uint16_t Ypos);
void LCD_DrawPoint(uint16_t x, uint16_t y);
uint16_t LCD_ReadPoint(uint16_t x, uint16_t y);
void LCD_SetWindows(uint16_t xStar, uint16_t yStar, uint16_t xEnd, uint16_t yEnd);

void LCD_WR_REG(uint16_t data);
void LCD_WR_DATA(uint16_t data);
uint16_t LCD_RD_DATA(void);
void LCD_WriteReg(uint16_t LCD_Reg, uint16_t LCD_RegValue);
void LCD_ReadReg(uint16_t LCD_Reg, uint8_t *Rval, int n);
void LCD_WriteRAM_Prepare(void);
void LCD_ReadRAM_Prepare(void);
void Lcd_WriteData_16Bit(uint16_t Data);
uint16_t Lcd_ReadData_16Bit(void);
void LCD_direction(uint8_t direction);
uint16_t LCD_Read_ID(void);
uint16_t LCD_read(void);

#ifdef __cplusplus
}
#endif

#endif /* __LCD_H */
