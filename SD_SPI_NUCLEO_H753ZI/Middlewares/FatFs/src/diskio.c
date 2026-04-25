#include "diskio.h"
#include "sd_spi.h"

#define SDDRV 0

static volatile DSTATUS Stat = STA_NOINIT;

DSTATUS disk_status(BYTE pdrv)
{
  if (pdrv != SDDRV) {
    return STA_NOINIT;
  }
  return Stat;
}

DSTATUS disk_initialize(BYTE pdrv)
{
  if (pdrv != SDDRV) {
    return STA_NOINIT;
  }

  if (SD_IO_Init()) {
    Stat &= (DSTATUS)~STA_NOINIT;
  } else {
    Stat = STA_NOINIT;
  }
  return Stat;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
  if ((pdrv != SDDRV) || !count || (Stat & STA_NOINIT)) {
    return RES_PARERR;
  }

  for (UINT i = 0; i < count; i++) {
    if (!SD_IO_ReadBlock(&buff[i * 512U], (uint32_t)((sector + i) * 512U))) {
      return RES_ERROR;
    }
  }
  return RES_OK;
}

#if _USE_WRITE == 1
DRESULT disk_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count)
{
  if ((pdrv != SDDRV) || !count || (Stat & STA_NOINIT)) {
    return RES_PARERR;
  }

  for (UINT i = 0; i < count; i++) {
    if (!SD_IO_WriteBlock((uint8_t *)&buff[i * 512U], (uint32_t)((sector + i) * 512U))) {
      return RES_ERROR;
    }
  }
  return RES_OK;
}
#endif

#if _USE_IOCTL == 1
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
  if (pdrv != SDDRV) {
    return RES_PARERR;
  }

  switch (cmd) {
    case CTRL_SYNC:
      return RES_OK;
    case GET_SECTOR_COUNT:
      *(DWORD *)buff = 0x10000U;
      return RES_OK;
    case GET_SECTOR_SIZE:
      *(WORD *)buff = 512U;
      return RES_OK;
    case GET_BLOCK_SIZE:
      *(DWORD *)buff = 1U;
      return RES_OK;
    default:
      return RES_PARERR;
  }
}
#endif

DWORD get_fattime(void)
{
  return ((DWORD)(2026 - 1980) << 25)
       | ((DWORD)1 << 21)
       | ((DWORD)1 << 16);
}
