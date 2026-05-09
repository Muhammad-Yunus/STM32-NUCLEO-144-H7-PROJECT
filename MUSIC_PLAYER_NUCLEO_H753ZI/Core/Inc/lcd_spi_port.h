#ifndef __LCD_SPI_PORT_H
#define __LCD_SPI_PORT_H

#include <stdint.h>
#include <stddef.h>
#include "stm32h7xx_hal.h"

/* -----------------------------------------------------------------------
 * lcd_spi_port — LCD/SPI hardware bridge (driver layer)
 *
 * Owns all GPIO/SPI interactions for the ILI9341 display.
 * simple_gfx.c depends on these; main.c must not define them.
 * ----------------------------------------------------------------------- */

/* Called once from main() after MX_SPI1_Init() and MX_GPIO_Init() */
void LCD_SPI_Init(SPI_HandleTypeDef *hspi);

/* Primitive I/O used internally by simple_gfx.c */
void LCD_WriteCmd(uint8_t cmd);
void LCD_WriteData(uint8_t data);
void LCD_WriteMultiData(uint8_t *data, size_t length);

/* ILI9341 address window helper */
void LCD_SetWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);


#endif /* __LCD_SPI_PORT_H */
