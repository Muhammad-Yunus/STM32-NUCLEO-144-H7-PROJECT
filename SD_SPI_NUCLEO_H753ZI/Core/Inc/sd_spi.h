/**
 * @file    sd_spi.h
 * @brief   SD Card SPI low-level driver interface for STM32H7
 */
#ifndef __SD_SPI_H
#define __SD_SPI_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32h7xx_hal.h"

/* Exported defines -----------------------------------------------------------*/
#define SD_CS_PORT GPIOE
#define SD_CS_PIN  GPIO_PIN_14

#define SD_BLOCK_SIZE 512

/* Exported functions --------------------------------------------------------*/
uint8_t  SD_IO_Init(void);
void    SD_IO_DeInit(void);
uint8_t  SD_IO_WriteByte(uint8_t data);
uint8_t  SD_IO_ReadByte(void);
uint8_t  SD_IO_WriteBlock(uint8_t* buf, uint32_t addr);
uint8_t  SD_IO_ReadBlock(uint8_t* buf, uint32_t addr);
void    SD_IO_Select(void);
void    SD_IO_Deselect(void);

#ifdef __cplusplus
}
#endif

#endif /* __SD_SPI_H */