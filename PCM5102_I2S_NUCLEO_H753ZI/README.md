# WAV Audio Player (16kHz) for NUCLEO-H753ZI + PCM5102A

This project implements a high-performance **microSD-based WAV player** and **audio test generator** for the **STM32H753ZI** using a **PCM5102A I2S DAC**.

## 1) Features
- **16kHz / 16-bit Stereo/Mono WAV Streaming** from microSD card.
- **Dynamic Selection**: Automatically plays `OASIS-~1.WAV` first, then falls back to the first available WAV.
- **Melody Fallback**: Plays a sine-wave melody if no SD card or valid WAV is found.
- **Test Mode**: Built-in fixed-frequency sine generator (default 1kHz) for hardware validation.
- **Optimization**: Uses SAI DMA circular buffering and high-speed SPI (15MHz) for glitch-free playback.
- **Audio Protection**: Hardware-aware clipping/saturation logic to prevent digital distortion.

## 2) Hardware Setup

### Connections (NUCLEO-H753ZI to PCM5102A / SD Card)

| Periperhal | Pin Name | NUCLEO-H753ZI | Function |
|---|---|---|---|
| **DAC (I2S)** | BCK | **PE5** | SAI1_SCK_A |
| | DIN | **PE6** | SAI1_SD_A |
| | LRCK | **PE4** | SAI1_FS_A |
| | VCC/GND | 3V3 / GND | Power |
| **SD (SPI)** | SCK | **PA5** | SPI1_SCK |
| | MISO / DO | **PA6** | SPI1_MISO (Internal Pull-up) |
| | MOSI / DI | **PB5** | SPI1_MOSI |
| | CS | **PE14** | SD_CS (Active Low) |

*Note: Ensure the PCM5102A board configuration (FMT/DEMP/MUTE pads) matches I2S standard.*

## 3) Configuration Macros (`main.c`)

- `AUDIO_OUTPUT_MODE`: Set to `AUDIO_MODE_WAV_OR_MELODY` (normal) or `AUDIO_MODE_TEST` (1kHz tone).
- `AUDIO_SAMPLE_RATE`: Fixed at `16000.0f` for current hardware clocking.
- `AUDIO_GAIN`: Volume multiplier for WAV playback.
- `AUDIO_AMPLITUDE`: Base amplitude for test tones.

## 4) Software Details
- **Kernel Clock**: SAI1 is clocked via PLL2 (fractional mode) to hit accurate audio frequencies.
- **File System**: FatFs over SPI1.
- **Resampling**: Files must be **16kHz**. 44.1k/48k files are rejected to ensure correct playback speed/pitch.

## 5) Usage
1. Format SD card as FAT32.
2. Place a 16kHz, 16-bit PCM WAV file named `OASIS-~1.WAV` (or any `.wav`) in the root.
3. Build and Flash using **STM32CubeIDE**.
4. Monitor progress via UART (COM1, 115200 8N1).

---
*Developed as a baseline for high-performance audio applications on STM32H7.*
