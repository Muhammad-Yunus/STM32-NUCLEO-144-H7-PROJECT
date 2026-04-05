/**
 ******************************************************************************
 * @file    sd_spi.h
 * @brief   Minimal SD card SPI-mode helper for STM32H753ZI
 *
 * Wiring (microSD module):
 *   SD_SCK   -> PA5  (SPI1_SCK)
 *   SD_MISO  -> PA6  (SPI1_MISO)
 *   SD_MOSI  -> PA7  (SPI1_MOSI)
 *   SD_CS    -> PB14 (GPIO output, active low)
 ******************************************************************************
 */

#ifndef __SD_SPI_H
#define __SD_SPI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"
#include <stdint.h>

extern SPI_HandleTypeDef hspi1;

/* CS pin */
#define SD_CS_PORT      GPIOB
#define SD_CS_PIN       GPIO_PIN_14

#define SD_CS_HIGH()    HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_SET)
#define SD_CS_LOW()     HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_RESET)

void SD_SPI_Init(void);
uint8_t SD_SPI_CardInit(void);
uint8_t SD_SPI_ReadCID(uint8_t cid[16]);

#ifdef __cplusplus
}
#endif

#endif /* __SD_SPI_H */
