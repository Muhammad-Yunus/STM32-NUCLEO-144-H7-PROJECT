#include "lcd_spi_port.h"
#include "main.h"
#include "ili9341.h"

static SPI_HandleTypeDef *s_lcd_spi = NULL;

static void SPI1_SendBlocking(const uint8_t *data, uint32_t len)
{
  if (s_lcd_spi == NULL) {
    return;
  }
  HAL_SPI_Transmit(s_lcd_spi, (uint8_t *)data, (uint16_t)len, 1000);
}

void LCD_WriteCmd(uint8_t cmd)
{
  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_RESET);
  SPI1_SendBlocking(&cmd, 1);
}

void LCD_WriteData(uint8_t data)
{
  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET);
  SPI1_SendBlocking(&data, 1);
}

void LCD_WriteMultiData(uint8_t *data, size_t length)
{
  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET);
  SPI1_SendBlocking(data, (uint32_t)length);
}

void LCD_SetWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
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


void LCD_SPI_Init(SPI_HandleTypeDef *hspi)
{
  s_lcd_spi = hspi;

  if (s_lcd_spi == NULL) {
    return;
  }

  s_lcd_spi->Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  HAL_SPI_Init(s_lcd_spi);

  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);

  LCD_WriteCmd(ILI9341_CMD_RST);
  HAL_Delay(120);

  ILI9341_Init(LCD_WriteData, LCD_WriteCmd);

  LCD_WriteCmd(ILI9341_CMD_MAC);
  LCD_WriteData(0x28);

  LCD_WriteCmd(ILI9341_CMD_PIXELFORMAT);
  LCD_WriteData(0x55);

  LCD_WriteCmd(ILI9341_CMD_SLEEPOUT);
  HAL_Delay(120);

  LCD_WriteCmd(ILI9341_CMD_DISPLAYON);
  HAL_Delay(20);

  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
}
