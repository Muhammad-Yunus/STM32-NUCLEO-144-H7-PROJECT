/**
 ******************************************************************************
 * @file    lcd.c
 * @brief   ST7796 3.5" 480x320 LCD driver using STM32H7 FMC (16-bit parallel)
 *
 * The LCD is connected to FMC Bank1 NE1 in Mode A (asynchronous SRAM-like).
 * RS (D/C) is wired to FMC_A0, so the REG address = 0x60000000 and
 * the DATA address = 0x60000002 (A0=1 in 16-bit mode → offset +2).
 *
 * Adapted for STM32 Nucleo-144 H753ZI from the lcdwiki reference project.
 ******************************************************************************
 */

#include "lcd.h"
#include "stm32h7xx_hal.h"

/* ── Globals ─────────────────────────────────────────────────────────────── */
static SRAM_HandleTypeDef SRAM_Handler;  /* Static - only used in init */

_lcd_dev lcddev;

uint16_t POINT_COLOR = 0x0000;   /* foreground: black */
uint16_t BACK_COLOR  = 0xFFFF;   /* background: white */

/* ═══════════════════════════════════════════════════════════════════════════
 *  Low-level bus read/write helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

uint16_t LCD_read(void)
{
    volatile uint16_t data;
    data = LCD->LCD_RAM;
    return data;
}

void LCD_WR_REG(uint16_t data)
{
    LCD->LCD_REG = data;
}

void LCD_WR_DATA(uint16_t data)
{
    LCD->LCD_RAM = data;
}

uint16_t LCD_RD_DATA(void)
{
    return LCD_read();
}

void LCD_WriteReg(uint16_t LCD_Reg, uint16_t LCD_RegValue)
{
    LCD->LCD_REG = LCD_Reg;
    LCD->LCD_RAM = LCD_RegValue;
}

void LCD_ReadReg(uint16_t LCD_Reg, uint8_t *Rval, int n)
{
    LCD_WR_REG(LCD_Reg);
    (void)LCD_RD_DATA();     /* Dummy read */
    while (n--)
    {
        *(Rval++) = (uint8_t)LCD_RD_DATA();
    }
}

void LCD_WriteRAM_Prepare(void)
{
    LCD_WR_REG(lcddev.wramcmd);
}

void LCD_ReadRAM_Prepare(void)
{
    LCD_WR_REG(lcddev.rramcmd);
}

void Lcd_WriteData_16Bit(uint16_t Data)
{
    LCD->LCD_RAM = Data;
}

uint16_t Lcd_ReadData_16Bit(void)
{
    uint16_t r;
    r = LCD_RD_DATA();       /* dummy read */
    r = LCD_RD_DATA();       /* real pixel */
    return r;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  MPU configuration – mark FMC SRAM region as device/write-through
 * ═══════════════════════════════════════════════════════════════════════════ */
static void LCD_MPU_Config(void)
{
    MPU_Region_InitTypeDef MPU_Initure;

    HAL_MPU_Disable();

    MPU_Initure.Enable            = MPU_REGION_ENABLE;
    MPU_Initure.Number            = LCD_REGION_NUMBER;
    MPU_Initure.BaseAddress       = LCD_ADDRESS_START;
    MPU_Initure.Size              = LCD_REGION_SIZE;
    MPU_Initure.SubRegionDisable  = 0x00;
    MPU_Initure.TypeExtField      = MPU_TEX_LEVEL0;
    MPU_Initure.AccessPermission  = MPU_REGION_FULL_ACCESS;
    MPU_Initure.DisableExec       = MPU_INSTRUCTION_ACCESS_ENABLE;
    MPU_Initure.IsShareable       = MPU_ACCESS_NOT_SHAREABLE;
    MPU_Initure.IsCacheable       = MPU_ACCESS_NOT_CACHEABLE;
    MPU_Initure.IsBufferable      = MPU_ACCESS_BUFFERABLE;
    HAL_MPU_ConfigRegion(&MPU_Initure);

    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  LCD_GPIO_FMC_Init – set up RST, BL pins and configure FMC peripheral
 * ═══════════════════════════════════════════════════════════════════════════ */
static void LCD_GPIO_FMC_Init(void)
{
    GPIO_InitTypeDef GPIO_Initure = {0};
    FMC_NORSRAM_TimingTypeDef ReadTim  = {0};
    FMC_NORSRAM_TimingTypeDef WriteTim = {0};

    /* ── RST pin: PA8 ───────────────────────────────────────────────────── */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_Initure.Pin   = LCD_RST_GPIO_PIN;
    GPIO_Initure.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_Initure.Pull  = GPIO_PULLUP;
    GPIO_Initure.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LCD_RST_GPIO_PORT, &GPIO_Initure);

    /* ── BL pin: PA9 ────────────────────────────────────────────────────── */
    GPIO_Initure.Pin = LCD_BL_GPIO_PIN;
    HAL_GPIO_Init(LCD_BL_GPIO_PORT, &GPIO_Initure);

    /* ── MPU region ─────────────────────────────────────────────────────── */
    LCD_MPU_Config();

    /* ── FMC SRAM handle ────────────────────────────────────────────────── */
    SRAM_Handler.Instance  = FMC_NORSRAM_DEVICE;
    SRAM_Handler.Extended  = FMC_NORSRAM_EXTENDED_DEVICE;

    SRAM_Handler.Init.NSBank              = FMC_NORSRAM_BANK1;
    SRAM_Handler.Init.DataAddressMux      = FMC_DATA_ADDRESS_MUX_DISABLE;
    SRAM_Handler.Init.MemoryType          = FMC_MEMORY_TYPE_SRAM;
    SRAM_Handler.Init.MemoryDataWidth     = FMC_NORSRAM_MEM_BUS_WIDTH_16;
    SRAM_Handler.Init.BurstAccessMode     = FMC_BURST_ACCESS_MODE_DISABLE;
    SRAM_Handler.Init.WaitSignalPolarity  = FMC_WAIT_SIGNAL_POLARITY_LOW;
    SRAM_Handler.Init.WaitSignalActive    = FMC_WAIT_TIMING_BEFORE_WS;
    SRAM_Handler.Init.WriteOperation      = FMC_WRITE_OPERATION_ENABLE;
    SRAM_Handler.Init.WaitSignal          = FMC_WAIT_SIGNAL_DISABLE;
    SRAM_Handler.Init.ExtendedMode        = FMC_EXTENDED_MODE_ENABLE;
    SRAM_Handler.Init.AsynchronousWait    = FMC_ASYNCHRONOUS_WAIT_DISABLE;
    SRAM_Handler.Init.WriteBurst          = FMC_WRITE_BURST_ENABLE;
    SRAM_Handler.Init.ContinuousClock     = FMC_CONTINUOUS_CLOCK_SYNC_ASYNC;
    SRAM_Handler.Init.WriteFifo           = FMC_WRITE_FIFO_ENABLE;
    SRAM_Handler.Init.PageSize            = FMC_PAGE_SIZE_NONE;

    /*
     * Timing for HCLK = 240 MHz → T_HCLK ≈ 4.17 ns
     * ST7796 requires minimum ~150ns write cycle
     */
    /* Read timing (conservative) */
    ReadTim.AddressSetupTime  = 0x06;   /* 6 HCLK = 25 ns */
    ReadTim.AddressHoldTime   = 0x03;   /* 3 HCLK = 12.5 ns */
    ReadTim.DataSetupTime     = 0x18;   /* 24 HCLK = 100 ns (safe for reads) */
    ReadTim.BusTurnAroundDuration = 0x02;
    ReadTim.AccessMode        = FMC_ACCESS_MODE_A;

    /* Write timing - aggressive for 240 MHz */
    WriteTim.AddressSetupTime = 0x01;   /* 1 HCLK = 4.2 ns */
    WriteTim.AddressHoldTime  = 0x00;
    WriteTim.DataSetupTime    = 0x06;   /* 6 HCLK = 25ns → total ~29ns */
    WriteTim.AccessMode       = FMC_ACCESS_MODE_A;

    HAL_SRAM_Init(&SRAM_Handler, &ReadTim, &WriteTim);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  LCD_Init
 * ═══════════════════════════════════════════════════════════════════════════ */
void LCD_Init(void)
{
    LCD_GPIO_FMC_Init();

    /* Power-on stabilization delay */
    HAL_Delay(50);

    /* Hardware reset - hold RST low for sufficient time */
    LCD_RST_LOW();
    HAL_Delay(120);   /* Increased from 100ms */
    LCD_RST_HIGH();
    HAL_Delay(120);   /* Increased from 100ms */

    /* ── ST7796S initialization sequence ────────────────────────────────────
     * Delays per ST7796 datasheet:
     * - Software reset (0x01): 120ms
     * - Sleep out (0x11): 120ms
     * - Display on (0x29): no delay required
     */
    HAL_Delay(50);              /* Power-on stabilization */

    LCD_WR_REG(0x01);           /* Software reset */
    HAL_Delay(120);             /* Datasheet spec: 120ms after reset */

    LCD_WR_REG(0x11);           /* Sleep OUT */
    HAL_Delay(120);             /* Datasheet spec: 120ms after sleep out */

    LCD_WR_REG(0x3A);           /* Interface Pixel Format */
    LCD_WR_DATA(0x55);          /* 16-bit RGB */

    LCD_WR_REG(0x36);           /* Memory Access Control */
    LCD_WR_DATA(0x08);          /* BGR=1 – blue-green-red order for ST7796 */

    LCD_WR_REG(0xB0);           /* Frame Rate Control */
    LCD_WR_DATA(0xA0);

    LCD_WR_REG(0xB1);           /* Frame Rate Control */
    LCD_WR_DATA(0x20);

    LCD_WR_REG(0xB2);           /* Frame Rate Control */
    LCD_WR_DATA(0x02); LCD_WR_DATA(0x02);

    LCD_WR_REG(0xB4);           /* Display Inversion */
    LCD_WR_DATA(0x00);          /* No inversion */

    LCD_WR_REG(0xC0);           /* Power Control 1 */
    LCD_WR_DATA(0x17); LCD_WR_DATA(0x15);

    LCD_WR_REG(0xC1);           /* Power Control 2 */
    LCD_WR_DATA(0x41);

    LCD_WR_REG(0xC2);           /* Power Control 3 */
    LCD_WR_DATA(0x44);

    LCD_WR_REG(0xC4);           /* VCOM Control */
    LCD_WR_DATA(0x15); LCD_WR_DATA(0x15);

    LCD_WR_REG(0xC6);           /* Frame Rate Control in Normal Mode */
    LCD_WR_DATA(0x0F);

    LCD_WR_REG(0xD0);           /* Power Control A */
    LCD_WR_DATA(0x8D); LCD_WR_DATA(0x81); LCD_WR_DATA(0x1C);

    LCD_WR_REG(0xE0);           /* PGAMCTRL (Positive Gamma) */
    LCD_WR_DATA(0xF0); LCD_WR_DATA(0x09); LCD_WR_DATA(0x0B); LCD_WR_DATA(0x06);
    LCD_WR_DATA(0x04); LCD_WR_DATA(0x15); LCD_WR_DATA(0x2F); LCD_WR_DATA(0x54);
    LCD_WR_DATA(0x42); LCD_WR_DATA(0x3C); LCD_WR_DATA(0x17); LCD_WR_DATA(0x14);
    LCD_WR_DATA(0x18); LCD_WR_DATA(0x1B);

    LCD_WR_REG(0xE1);           /* NGAMCTRL (Negative Gamma) */
    LCD_WR_DATA(0xE0); LCD_WR_DATA(0x09); LCD_WR_DATA(0x0B); LCD_WR_DATA(0x06);
    LCD_WR_DATA(0x04); LCD_WR_DATA(0x03); LCD_WR_DATA(0x2B); LCD_WR_DATA(0x43);
    LCD_WR_DATA(0x42); LCD_WR_DATA(0x3B); LCD_WR_DATA(0x16); LCD_WR_DATA(0x14);
    LCD_WR_DATA(0x17); LCD_WR_DATA(0x1B);

    LCD_WR_REG(0x11);           /* Sleep OUT */
    HAL_Delay(120);

    LCD_WR_REG(0x29);           /* Display ON */

    /* Set direction and clear - LCD_direction() handles column/row addresses */
    LCD_direction(USE_HORIZONTAL);

    LCD_BL_ON();
    LCD_Clear(BLACK);

}

/* ═══════════════════════════════════════════════════════════════════════════
 *  High-level drawing primitives
 * ═══════════════════════════════════════════════════════════════════════════ */

void LCD_SetWindows(uint16_t xStar, uint16_t yStar, uint16_t xEnd, uint16_t yEnd)
{
    LCD_WR_REG(lcddev.setxcmd);
    LCD_WR_DATA(xStar >> 8);
    LCD_WR_DATA(xStar & 0xFF);
    LCD_WR_DATA(xEnd  >> 8);
    LCD_WR_DATA(xEnd  & 0xFF);

    LCD_WR_REG(lcddev.setycmd);
    LCD_WR_DATA(yStar >> 8);
    LCD_WR_DATA(yStar & 0xFF);
    LCD_WR_DATA(yEnd  >> 8);
    LCD_WR_DATA(yEnd  & 0xFF);

    LCD_WriteRAM_Prepare();
}

void LCD_SetCursor(uint16_t Xpos, uint16_t Ypos)
{
    LCD_SetWindows(Xpos, Ypos, Xpos, Ypos);
}

void LCD_DrawPoint(uint16_t x, uint16_t y)
{
    LCD_SetCursor(x, y);
    Lcd_WriteData_16Bit(POINT_COLOR);
}

uint16_t LCD_ReadPoint(uint16_t x, uint16_t y)
{
    if (x >= lcddev.width || y >= lcddev.height)
        return 0;
    LCD_SetCursor(x, y);
    LCD_ReadRAM_Prepare();
    return Lcd_ReadData_16Bit();
}

void LCD_Clear(uint16_t Color)
{
    uint32_t total = (uint32_t)lcddev.width * lcddev.height;
    LCD_SetWindows(0, 0, lcddev.width - 1, lcddev.height - 1);
    for (uint32_t i = 0; i < total; i++)
    {
        LCD->LCD_RAM = Color;
    }
}

void LCD_direction(uint8_t direction)
{
    lcddev.setxcmd  = 0x2A;
    lcddev.setycmd  = 0x2B;
    lcddev.wramcmd  = 0x2C;
    lcddev.rramcmd  = 0x2E;

    switch (direction)
    {
        case 0:
            lcddev.width  = LCD_W;
            lcddev.height = LCD_H;
            /* Portrait – MX mirror for correct text direction, BGR */
            LCD_WriteReg(0x36, (1 << 6) | (1 << 3));   /* MX=1, BGR=1 */
            break;
        case 1:
            lcddev.width  = LCD_H;
            lcddev.height = LCD_W;
            /* 90° rotation – MV=1, BGR=1 (no MX mirror) */
            LCD_WriteReg(0x36, (1 << 5) | (1 << 3)); /* MV, BGR */
            break;
        case 2:
            lcddev.width  = LCD_W;
            lcddev.height = LCD_H;
            /* 180° – MX=1, MY=1, BGR=1 */
            LCD_WriteReg(0x36, (1 << 6) | (1 << 7) | (1 << 3)); /* MX, MY, BGR */
            break;
        case 3:
            lcddev.width  = LCD_H;
            lcddev.height = LCD_W;
            /* 270° – MY=1, MV=1, BGR=1 */
            LCD_WriteReg(0x36, (1 << 7) | (1 << 5) | (1 << 3) | (1 << 6)); /* MY, MX, MV, BGR */
            break;
        default:
            break;
    }
}

uint16_t LCD_Read_ID(void)
{
    uint8_t val[4] = {0};
    LCD_ReadReg(0xD3, val, 4);
    /* ST7796 returns 0x00, 0x00, 0x77, 0x96 */
    return (uint16_t)((val[2] << 8) | val[3]);
}
