#include "sd_spi.h"

extern SPI_HandleTypeDef hspi1;

#define SD_DUMMY   0xFFU
#define SD_TOK_RD  0xFEU   /* single block read token */
#define SD_TOK_WR  0x05U   /* data accepted token */

/* Commands */
#define CMD0_GO_IDLE    0x00U
#define CMD1_SEND_OP 0x01U
#define CMD8_SEND_IF 0x08U
#define CMD16_SET_BLK 0x10U
#define CMD17_READ   0x11U
#define CMD24_WRITE 0x18U
#define CMD55_APP   0x37U
#define CMD58_READ_OCR 0x3AU
#define ACMD41_SD_SEND_OP 0x29U

/* CRC for CMD0 (0x00) with CRC7=0x00 -> 0x95 */
#define CRC_CMD0 0x95U
#define CRC_CMD8 0x87U

/* Helpers */
static void sd_send_dummy(uint8_t n)
{
  for (uint8_t i = 0; i < n; i++) {
    SD_IO_WriteByte(SD_DUMMY);
  }
}

uint8_t SD_IO_WriteByte(uint8_t byte)
{
  uint8_t rx = SD_DUMMY;
  if (HAL_OK != HAL_SPI_TransmitReceive(&hspi1, &byte, &rx, 1, 10)) {
    return 0xFFU;
  }
  return rx;
}

uint8_t SD_IO_ReadByte(void)
{
  return SD_IO_WriteByte(SD_DUMMY);
}

void SD_IO_Select(void)
{
  HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_RESET);
}

void SD_IO_Deselect(void)
{
  HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_SET);
  sd_send_dummy(1);
}

static uint8_t sd_wait_ready(uint32_t ms)
{
  uint32_t to = HAL_GetTick() + ms;
  while ((int32_t)(HAL_GetTick() - to) < 0) {
    if (SD_IO_ReadByte() == SD_DUMMY) {
      return 1U;
    }
  }
  return 0U;
}

static uint8_t sd_cmd(uint8_t cmd, uint32_t arg, uint8_t crc)
{
  uint8_t frame[6];
  uint8_t resp, i;

  frame[0] = 0x40U | cmd;
  frame[1] = (uint8_t)(arg >> 24);
  frame[2] = (uint8_t)(arg >> 16);
  frame[3] = (uint8_t)(arg >> 8);
  frame[4] = (uint8_t)arg;
  frame[5] = crc;

  SD_IO_WriteByte(SD_DUMMY);
  for (i = 0; i < 6; i++) {
    SD_IO_WriteByte(frame[i]);
  }

  for (i = 0; i < 32; i++) {
    resp = SD_IO_ReadByte();
    if ((resp & 0x80U) == 0U) {
      return resp;
    }
  }
  return 0xFFU;
}

/*----- Public API -----*/

static uint8_t g_sd_block_addressing = 0U;

uint8_t SD_IO_Init(void)
{
  uint8_t r1, attempts;
  uint8_t ocr[4];
  uint8_t sd_type = 0;   /* 0=unknown, 1=SDv1, 2=SDv2, 3=SDHC */


  /* 1. Start up: >= 74 dummy clocks with CS high */
  SD_IO_Deselect();
  sd_send_dummy(200);

  /* 2. CMD0: enter idle (retry, some cards need multiple attempts) */
  r1 = 0xFFU;
  for (attempts = 0; attempts < 10U; attempts++) {
    SD_IO_Deselect();
    sd_send_dummy(10);
    SD_IO_Select();
    sd_send_dummy(2);
    r1 = sd_cmd(CMD0_GO_IDLE, 0U, CRC_CMD0);
    SD_IO_Deselect();
    if (r1 == 0x01U) {
      break;
    }
    sd_send_dummy(8);
  }

  if (r1 != 0x01U) {
    return 0U;
  }

  /* 3. CMD8: check SDv2 (only for SD) */
  SD_IO_Deselect();
  sd_send_dummy(2);
  SD_IO_Select();
  r1 = sd_cmd(CMD8_SEND_IF, 0x1AAU, CRC_CMD8);
  if (r1 == 0x01U) {
    for (uint8_t i = 0; i < 4; i++) {
      ocr[i] = SD_IO_ReadByte();
    }
    SD_IO_Deselect();
    if ((ocr[2] == 0x01U) && (ocr[3] == 0xAAU)) {
      sd_type = 2;
    } else {
      return 0U;
    }
  } else {
    /* Not error, just SDv1 or MMC */
    SD_IO_Deselect();
    sd_type = 1;
  }

  /* 4. ACMD41 (SDv2) or CMD1 (SDv1/MMC) */
  attempts = 0;
  do {
    if (sd_type >= 2) {
      SD_IO_Deselect();
      sd_send_dummy(2);
      SD_IO_Select();
      (void)sd_cmd(CMD55_APP, 0U, 0x01U);
      r1 = sd_cmd(ACMD41_SD_SEND_OP, 0x40000000U, 0x01U);
    } else {
      SD_IO_Deselect();
      sd_send_dummy(2);
      SD_IO_Select();
      r1 = sd_cmd(CMD1_SEND_OP, 0U, 0x01U);
    }
    SD_IO_Deselect();

    attempts++;
    if (attempts > 200) {
      return 0U;
    }
    HAL_Delay(2);
  } while (r1 != 0x00U);


  /* 5. Check CCS (SDHC) via CMD58 for SDv2 */
  if (sd_type >= 2) {
    SD_IO_Deselect();
    sd_send_dummy(2);
    SD_IO_Select();
    r1 = sd_cmd(CMD58_READ_OCR, 0U, 0x01U);
    if (r1 == 0x00U) {
      for (uint8_t i = 0; i < 4U; i++) {
        ocr[i] = SD_IO_ReadByte();
      }
      sd_type = (ocr[0] & 0x40U) ? 3 : 2;
    }
    SD_IO_Deselect();
  }

  g_sd_block_addressing = (sd_type >= 3U) ? 1U : 0U;

  /* 6. Set block length if not SDHC */
  if (sd_type < 3U) {
    SD_IO_Select();
    (void)sd_cmd(CMD16_SET_BLK, 512U, 0x01U);
    SD_IO_Deselect();
  }

  return 1U;
}

void SD_IO_DeInit(void)
{
  SD_IO_Deselect();
}

/* Simple block read: addr is logical sector number */
uint8_t SD_IO_ReadBlock(uint8_t *buf, uint32_t addr)
{
  uint8_t r1, token;
  uint32_t to = HAL_GetTick() + 200U;

  if (!g_sd_block_addressing) {
    addr *= 512U;
  }

  SD_IO_Deselect();
  sd_send_dummy(2);
  SD_IO_Select();

  r1 = sd_cmd(CMD17_READ, addr, 0x01U);
  if (r1 != 0x00U) {
    r1 = sd_cmd(CMD17_READ, addr, 0x01U);
    if (r1 != 0x00U) {
      SD_IO_Deselect();
      return 0U;
    }
  }

  while ((int32_t)(HAL_GetTick() - to) < 0) {
    token = SD_IO_ReadByte();
    if (token != SD_DUMMY) {
      break;
    }
  }

  if (token != SD_TOK_RD) {
    SD_IO_Deselect();
    return 0U;
  }

  for (uint16_t i = 0; i < 512U; i++) {
    buf[i] = SD_IO_ReadByte();
  }

  (void)SD_IO_ReadByte();   /* CRC */
  (void)SD_IO_ReadByte();

  SD_IO_Deselect();
  return 1U;
}

uint8_t SD_IO_WriteBlock(uint8_t *buf, uint32_t addr)
{
  uint8_t r1, resp;
  uint8_t tries;

  if (!g_sd_block_addressing) {
    addr *= 512U;
  }

  SD_IO_Deselect();
  sd_send_dummy(2);
  SD_IO_Select();
  r1 = sd_cmd(CMD24_WRITE, addr, 0x01U);
  if (r1 != 0x00U) {
    r1 = sd_cmd(CMD24_WRITE, addr, 0x01U);
    if (r1 != 0x00U) {
      SD_IO_Deselect();
      return 0U;
    }
  }

  SD_IO_WriteByte(0xFEU);
  for (uint16_t i = 0; i < 512U; i++) {
    SD_IO_WriteByte(buf[i]);
  }
  SD_IO_WriteByte(SD_DUMMY);
  SD_IO_WriteByte(SD_DUMMY);

  resp = 0xFFU;
  for (tries = 0; tries < 16U; tries++) {
    resp = SD_IO_ReadByte();
    if (resp == SD_DUMMY) {
      continue;
    }
    break;
  }

  if ((resp & 0x1FU) != SD_TOK_WR) {
    SD_IO_Deselect();
    return 0U;
  }

  if (!sd_wait_ready(500U)) {
    SD_IO_Deselect();
    return 0U;
  }

  SD_IO_Deselect();

  return 1U;
}
