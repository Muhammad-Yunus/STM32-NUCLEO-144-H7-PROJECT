# ST7796 16-bit FMC Demo on NUCLEO-H753ZI

## 1) Project summary

This project brings up a **3.5" ST7796 TFT LCD** with **XPT2046 resistive touch** on **STM32 NUCLEO-H753ZI** using a **16-bit parallel FMC bus**.

Features:
- ST7796 LCD driver with 16-bit FMC interface
- LVGL v9 integration with music demo
- Optimized 27-28 FPS performance
- Touch driver with calibration support
- 480 MHz system clock with I/D-Cache enabled

Target board: **STM32H753ZI (NUCLEO-144)**

---

## 2) Hardware Requirements

| Component | Description |
|-----------|-------------|
| MCU | STM32H753ZI (480 MHz Cortex-M7) |
| LCD | 3.5" ST7796 16-bit parallel TFT |
| Touch | XPT2046 resistive touch controller |
| Interface | FMC/FSMC 16-bit parallel bus |

---

## 3) Pin Mapping

### 3.1 LCD Data Bus (16-bit FMC)

| LCD Pin | STM32 Pin | FMC Signal | Notes |
|---------|-----------|------------|-------|
| DB0 | PD14 | FMC_D0 | Data bus LSB |
| DB1 | PD15 | FMC_D1 | |
| DB2 | PD0 | FMC_D2 | |
| DB3 | PD1 | FMC_D3 | |
| DB4 | PE7 | FMC_D4 | |
| DB5 | PE8 | FMC_D5 | |
| DB6 | PE9 | FMC_D6 | |
| DB7 | PE10 | FMC_D7 | |
| DB8 | PE11 | FMC_D8 | |
| DB9 | PE12 | FMC_D9 | |
| DB10 | PE13 | FMC_D10 | |
| DB11 | PE14 | FMC_D11 | |
| DB12 | PE15 | FMC_D12 | |
| DB13 | PD8 | FMC_D13 | |
| DB14 | PD9 | FMC_D14 | |
| DB15 | PD10 | FMC_D15 | Data bus MSB |

### 3.2 LCD Control Signals

| LCD Pin | STM32 Pin | FMC Signal | Function |
|---------|-----------|------------|----------|
| WR | PD5 | FMC_NWE | Write enable (active LOW) |
| RD | PD4 | FMC_NOE | Read enable (active LOW) |
| RS/DC | PF0 | FMC_A0 | Register select (D/C) |
| CS | PD7 | FMC_NE1 | Chip select (active LOW) |
| RST | PA8 | GPIO | Reset (active LOW) |
| BL | PA9 | GPIO | Backlight (active HIGH) |

> **Important**: PC8/PC9 on NUCLEO-H753ZI are **DNF** (Do Not Fit). Use PA8/PA9 for RST/BL.

### 3.3 Touch (XPT2046 Bit-Bang SPI)

| Touch Pin | STM32 Pin | Signal | Direction |
|-----------|-----------|--------|-----------|
| T_CLK | PB3 | GPIO Output | SCK |
| T_MISO | PB4 | GPIO Input | MISO |
| T_MOSI | PB5 | GPIO Output | MOSI |
| T_CS | PB6 | GPIO Output | Chip select |
| T_PEN | PG9 | GPIO Input | Touch detect IRQ |

---

## 4) Project Setup

### 4.1 Prerequisites

1. **STM32CubeIDE** (v1.18.x or later)
2. **STM32CubeMX** (included in CubeIDE)
3. **STM32CubeH7** firmware package
4. **LVGL v9** (cloned into `Middlewares/lvgl` folder)

### 4.2 Clone LVGL v9

```bash
mkdir Middlewares
cd Middlewares
git clone https://github.com/lvgl/lvgl.git
cd lvgl
git checkout v9.2.1  # or latest v9.x release
```

### 4.3 STM32CubeMX Configuration

1. Open `.ioc` file in STM32CubeMX
2. Configure **FMC**:
   - Mode: SRAM/PSRAM Chip Select
   - Bank: NE1
   - Data/Address: D0-D15, A0
   - Control: NOE, NWE, NE1
3. Configure **GPIO**:
   - PA8: Output (RST)
   - PA9: Output (BL)
   - PB3, PB5, PB6: Output (Touch CLK, MOSI, CS)
   - PB4, PG9: Input (Touch MISO, PEN)
4. Disable **VCP** to free PD8/PD9:
   - `NUCLEO-H753ZI.VCP=false` in .ioc
5. Generate Code

### 4.4 Project Settings in STM32CubeIDE

1. **Include Paths**: Add to C/C++ Build → Settings → MCU GCC → Include paths:
   - `Middlewares/lvgl`
   - `Middlewares/lvgl/src`

2. **Preprocessor Defines**: Ensure `LV_CONF_INCLUDE_SIMPLE` is NOT defined

3. **Linker**: Verify `RAM_D2` section exists in linker script

---

## 5) Build Instructions

1. Import project into STM32CubeIDE
2. Clean project: `Project → Clean...`
3. Build: `Project → Build All` (or Ctrl+B)

Expected output:
- `Debug/ST7796_16BIT_NUCLEO_H753ZI.elf`
- Size: ~300-400 KB (includes LVGL library)

---

## 6) Flash & Debug

### Option A: STM32CubeIDE
1. Connect NUCLEO-H753ZI via USB
2. Debug configuration → Run

### Option B: STM32CubeProgrammer CLI
```bash
STM32_Programmer_CLI.exe -c port=SWD -w Debug/ST7796_16BIT_NUCLEO_H753ZI.elf -v -rst
```

---

## 7) Key Features

### 7.1 Performance Optimizations

| Feature | Implementation | Benefit |
|---------|----------------|---------|
| FMC Burst Mode | Enabled in `lcd.c` | Faster bus transfers |
| Large Buffers | 120-line double buffering | Fewer flushes per frame |
| Fast Pixel Loop | Inline functions, aligned access | Minimal per-pixel overhead |
| Aggressive Timing | 29ns write cycle | Stable at 480 MHz |
| LVGL Optimization | Reduced logging, fast refresh | 48 FPS achieved |

### 7.2 Clock Configuration
- System Clock: **480 MHz** (PLL from HSE via ST-LINK MCO 8 MHz)
- Voltage Scale: **VOS0** (required for 480 MHz)
- I-Cache / D-Cache: **Enabled**
- HCLK: **240 MHz** (AHB divider = 2)
- APB1/2/3: **120 MHz**
- FMC Clock: **240 MHz** (HCLK)

### 7.3 Memory Usage
- `RAM_D2`: LVGL buffers (120 lines × 480 × 2 bytes × 2 = ~230KB)
- `RAM_D1`: Stack, heap, global variables

---

## 8) Project Structure

```
ST7796_16BIT_NUCLEO_H753ZI/
├── Core/
│   ├── Inc/
│   │   ├── lcd.h              # ST7796 driver header
│   │   ├── touch.h            # XPT2046 touch header
│   │   ├── lv_conf.h          # LVGL configuration
│   │   ├── lv_port_disp.h     # LVGL display port
│   │   └── lv_port_indev.h    # LVGL input port
│   └── Src/
│       ├── main.c              # Application entry
│       ├── lcd.c                 # ST7796 low-level driver
│       ├── touch.c               # XPT2046 touch driver
│       ├── lv_port_disp.c        # LVGL display flush
│       └── lv_port_indev.c       # LVGL touch input
├── Middlewares/
│   └── lvgl/                   # LVGL v9 library
│       ├── lvgl.h
│       ├── src/
│       └── demos/
├── Drivers/
│   ├── BSP/                    # Board support
│   ├── CMSIS/
│   └── STM32H7xx_HAL_Driver/   # HAL library
├── STM32H753ZITX_FLASH.ld      # Linker script
└── README.md                   # This file
```

---

## 9) Performance Results

| Metric | Value |
|--------|-------|
| **FPS (music demo)** | **48 FPS** |
| System Clock | 480 MHz |
| LCD Interface | 16-bit parallel FMC |
| Buffer Size | 120 lines double buffered |
| Pixel Cycle | ~29 ns |
| Theoretical Max | ~60+ FPS (scaling with clock) |

The 48 FPS was achieved with 480 MHz system clock, 240 MHz HCLK, and optimized FMC timing.

---

## 10) Troubleshooting

### 10.1 Black Screen
- Check backlight (PA9 = HIGH)
- Verify FMC pins in .ioc match `lcd.h`
- Ensure `VCP=false` to free PD8/PD9

### 10.2 Touch Not Working
- Check wiring to PB3-PB6, PG9
- Run calibration: call `TP_Adjust()` in main.c

### 10.3 Low FPS
- Verify I/D-Cache enabled: `SCB_EnableICache()`, `SCB_EnableDCache()`
- Check `RAM_D2` section in linker script
- Disable LVGL logging in `lv_conf.h`
- Verify 480 MHz PLL config in `main.c` (HSE bypass, VOS0, PLLN=60, PLLP=1)

### 10.4 Build Errors
- Ensure LVGL v9 cloned to `Middlewares/lvgl`
- Add include paths in project settings
- Clean and rebuild after .ioc changes

---

## 11) References

- [LVGL Documentation](https://docs.lvgl.io/)
- [STM32H7 Reference Manual](https://www.st.com/resource/en/reference_manual/rm0433-stm32h742-and-stm32h753755-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)
- [ST7796 Datasheet](https://www.displayfuture.com/Display/datasheet/controller/ST7796s.pdf)
- [FMC Application Note](https://www.st.com/resource/en/application_note/an2784-using-the-stm32f2f4f7-series-fmc-controller-for-external-memory-access-stmicroelectronics.pdf)

---

## 12) License

This project is provided as-is for educational and development purposes.
LVGL is licensed under MIT License.
STM32 HAL is provided by STMicroelectronics under their license terms.

---

**Last Updated**: 2026-04-05
**Tested With**: STM32CubeIDE 1.18.1, STM32CubeH7 v1.12.0, LVGL v9.2.1
