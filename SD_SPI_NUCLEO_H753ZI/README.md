# Basic Success Example: SD Card over SPI on STM32H753ZI

This project demonstrates a working baseline for **microSD over SPI + FatFs** on **NUCLEO-H753ZI** using STM32CubeIDE.

Current validated runtime output:

```text
--- STM32H7 SD Card Test ---
SD Init: OK
Mount: OK
Wrote 20 bytes to test.txt
Read (20): Hello from STM32H7!
---
TEST.TXT     20
---
Done.
```

## 1) Project Summary

- Board: **NUCLEO-H753ZI** (STM32H753ZIT6)
- Toolchain: **STM32CubeIDE**
- Firmware package: **STM32Cube FW_H7 V1.12.1**
- Storage stack:
  - Low-level SD SPI driver: `Core/Src/sd_spi.c`
  - FatFs core: `Core/Src/ff.c` + `Core/Src/diskio.c`
  - App test flow: `Core/Src/main.c`

## 2) Pin Mapping Used

### SPI bus (SD module)

- `SPI1_SCK`  -> **PA5**
- `SPI1_MISO` -> **PA6** (configured with pull-up in MSP init)
- `SPI1_MOSI` -> **PB5**
- `SD_CS`     -> **PE14** (Arduino D4)


## 3) Wiring Guide (Dedicated SD Module)

| SD module pin | NUCLEO-H753ZI pin |
|---|---|
| SCK | PA5 |
| MISO / DO | PA6 |
| MOSI / DI | PB5 |
| CS | PE14 |
| VCC | 3V3 |
| GND | GND |

Important:
- Use a **3.3V compatible** SD module.
- Keep wires short.
- Ensure common ground.

## 4) SPI / SD Settings

- SPI mode used: CPOL low, CPHA 1st edge (Mode 0)
- 8-bit transfers
- Slow init clock (prescaler 256)
- SD init sequence includes CMD0/CMD8/ACMD41/CMD58 with robust retries and timing gaps.

## 5) Files of Interest

- `Core/Src/main.c`
  - UART banner
  - SD init + mount
  - write/read/list test flow
- `Core/Src/sd_spi.c`
  - SPI byte I/O
  - SD command engine
  - block read/write
- `Core/Inc/sd_spi.h`
  - SD CS pin definition (`PE14`)
- `Core/Src/diskio.c`
  - FatFs disk glue (sector read/write/ioctl)
- `Core/Inc/ffconf.h`
  - FatFs config

## 6) Build and Flash

### In STM32CubeIDE

1. Open project `SD_SPI_NUCLEO_H753ZI`.
2. **Project -> Clean**.
3. Build (`Ctrl+B`).
4. Flash/run (`Run` / `Debug`).
5. Open VCP terminal at **115200 8N1**.

### Expected startup behavior

- SD initializes successfully
- FatFs mount succeeds
- `test.txt` is created, written, read back, and listed

## 7) Troubleshooting

- If `SD Init: FAILED`
  - verify CS pin is correct and toggling
  - check MOSI/MISO not swapped
  - verify module voltage and grounding
- If `Mount: FAILED`
  - verify disk I/O glue in `diskio.c`
  - card may need formatting (FAT32)
- If write/read intermittently fails
  - keep non-target SPI slaves deselected
  - shorten wiring

## 8) Notes

This repository state is intended as a **basic successful reference** for SD SPI on STM32H753ZI before adding higher-level application logic.
