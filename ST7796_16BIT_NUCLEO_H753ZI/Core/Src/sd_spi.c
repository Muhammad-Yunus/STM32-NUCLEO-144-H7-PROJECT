/**
 ******************************************************************************
 * @file    sd_spi.c
 * @brief   Minimal SD card SPI-mode helper for STM32H753ZI
 *
 * This is a simple bring-up layer:
 *   - configure CS pin
 *   - send SD init sequence (CMD0, CMD8, ACMD41, CMD58)
 *   - read CID register (CMD10)
 *
 * It is intended for hardware validation/demo, not full FatFs yet.
 ******************************************************************************
 */

#include "sd_spi.h"

/* ── Local helpers ───────────────────────────────────────────────────────── */
static uint8_t sd_spi_txrx(uint8_t data)
{
    uint8_t rx = 0xFF;
    (void)HAL_SPI_TransmitReceive(&hspi1, &data, &rx, 1, HAL_MAX_DELAY);
    return rx;
}

static void sd_spi_dummy_clocks(uint16_t cycles)
{
    while (cycles--)
    {
        (void)sd_spi_txrx(0xFF);
    }
}

static uint8_t sd_spi_wait_r1(uint32_t timeout_bytes)
{
    uint8_t r;
    do
    {
        r = sd_spi_txrx(0xFF);
        if ((r & 0x80U) == 0) return r;
    } while (timeout_bytes--);
    return 0xFF;
}

static uint8_t sd_spi_send_cmd(uint8_t cmd, uint32_t arg, uint8_t crc)
{
    SD_CS_LOW();
    (void)sd_spi_txrx(0xFF);

    (void)sd_spi_txrx((uint8_t)(0x40U | cmd));
    (void)sd_spi_txrx((uint8_t)(arg >> 24));
    (void)sd_spi_txrx((uint8_t)(arg >> 16));
    (void)sd_spi_txrx((uint8_t)(arg >> 8));
    (void)sd_spi_txrx((uint8_t)arg);
    (void)sd_spi_txrx(crc);

    return sd_spi_wait_r1(10000);
}

static void sd_spi_end_cmd(void)
{
    SD_CS_HIGH();
    (void)sd_spi_txrx(0xFF);
}

/* ── Public API ──────────────────────────────────────────────────────────── */
void SD_SPI_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitStruct.Pin   = SD_CS_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(SD_CS_PORT, &GPIO_InitStruct);

    SD_CS_HIGH();
    sd_spi_dummy_clocks(16); /* >=74 clocks with CS high */
}

uint8_t SD_SPI_CardInit(void)
{
    uint8_t r1;
    uint32_t t;

    /* 1) Enter idle with CMD0 */
    for (t = 0; t < 20; t++)
    {
        r1 = sd_spi_send_cmd(0, 0x00000000UL, 0x95U); /* CMD0 valid CRC */
        sd_spi_end_cmd();
        if (r1 == 0x01U) break;
    }
    if (r1 != 0x01U) return 1;

    /* 2) CMD8: check SD v2 and voltage range */
    r1 = sd_spi_send_cmd(8, 0x000001AAUL, 0x87U);
    if (r1 != 0x01U)
    {
        sd_spi_end_cmd();
        return 2;
    }
    /* Read R7 (4 bytes), ignore content in this minimal demo */
    (void)sd_spi_txrx(0xFF);
    (void)sd_spi_txrx(0xFF);
    (void)sd_spi_txrx(0xFF);
    (void)sd_spi_txrx(0xFF);
    sd_spi_end_cmd();

    /* 3) ACMD41 loop (CMD55 + CMD41(HCS)) until ready */
    for (t = 0; t < 20000; t++)
    {
        r1 = sd_spi_send_cmd(55, 0x00000000UL, 0x01U);
        sd_spi_end_cmd();
        if (r1 > 0x01U) return 3;

        r1 = sd_spi_send_cmd(41, 0x40000000UL, 0x01U);
        sd_spi_end_cmd();
        if (r1 == 0x00U) break;
    }
    if (r1 != 0x00U) return 4;

    /* 4) CMD58 read OCR (optional for demo) */
    r1 = sd_spi_send_cmd(58, 0x00000000UL, 0x01U);
    if (r1 != 0x00U)
    {
        sd_spi_end_cmd();
        return 5;
    }
    (void)sd_spi_txrx(0xFF);
    (void)sd_spi_txrx(0xFF);
    (void)sd_spi_txrx(0xFF);
    (void)sd_spi_txrx(0xFF);
    sd_spi_end_cmd();

    return 0;
}

uint8_t SD_SPI_ReadCID(uint8_t cid[16])
{
    uint8_t r1;
    uint32_t t;

    if (cid == NULL) return 1;

    r1 = sd_spi_send_cmd(10, 0x00000000UL, 0x01U); /* CMD10 */
    if (r1 != 0x00U)
    {
        sd_spi_end_cmd();
        return 2;
    }

    /* Wait data token 0xFE */
    for (t = 0; t < 100000; t++)
    {
        uint8_t token = sd_spi_txrx(0xFF);
        if (token == 0xFEU) break;
    }
    if (t >= 100000)
    {
        sd_spi_end_cmd();
        return 3;
    }

    for (uint8_t i = 0; i < 16; i++)
        cid[i] = sd_spi_txrx(0xFF);

    /* CRC bytes */
    (void)sd_spi_txrx(0xFF);
    (void)sd_spi_txrx(0xFF);

    sd_spi_end_cmd();
    return 0;
}
