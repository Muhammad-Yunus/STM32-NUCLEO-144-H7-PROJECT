/**
 * @file lv_conf.h
 * Configuration file for LVGL v9
 *
 * Adapted for STM32H753ZI with ST7796 16-bit parallel LCD (480x320 landscape)
 */

#ifndef LV_CONF_H
#define LV_CONF_H

/* clang-format off */

/* ============================
 * System Performance
 * ============================ */

/* Memory management for LVGL */
#define LV_USE_STDLIB_MALLOC    LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_STRING    LV_STDLIB_BUILTIN

#define LV_MEM_SIZE             (96U * 1024U)

/* Use large buffers for display to improve FPS */
#define LV_USE_LARGE_HOR_RES    32U

/* HAL tick period */
#define LV_DEF_REFR_PERIOD     12

/* Performance monitor - useful for tuning */
#define LV_USE_SYSMON           1
#define LV_USE_PERF_MONITOR     1

/* ============================
 * DMA2D GPU Acceleration
 * ============================ */

/* DMA2D is disabled - LVGL's DMA2D driver conflicts with
 * the existing HAL DMA2D initialization for FMC LCD access.
 */
/* #define LV_USE_DRAW_DMA2D */
/* #define LV_DRAW_DMA2D_HAL_INCLUDE */
/* #define LV_USE_DRAW_DMA2D_INTERRUPT */

/* ============================
 * Demos
 * ============================ */

#define LV_USE_ANIMIMAGE        1
#define LV_USE_FLEX             1
#define LV_USE_GRID             1

#define LV_USE_DEMO_WIDGETS     0
#define LV_USE_DEMO_BENCHMARK   0
#define LV_USE_DEMO_STRESS      0

#define LV_USE_DEMO_MUSIC       1
#if LV_USE_DEMO_MUSIC
# define LV_DEMO_MUSIC_SQUARE       0
# define LV_DEMO_MUSIC_LANDSCAPE    1
# define LV_DEMO_MUSIC_ROUND        0
# define LV_DEMO_MUSIC_LARGE        0
# define LV_DEMO_MUSIC_AUTO_PLAY    0
#endif

#define LV_USE_DEMO_FLEX_LAYOUT     0
#define LV_USE_DEMO_MULTILANG       0
#define LV_USE_DEMO_TRANSFORM       0
#define LV_USE_DEMO_SCROLL          0

/* ============================
 * Display Configuration
 * ============================ */

/*
 * FMC 16-bit parallel interface:
 * The ST7796 LCD expects RGB565 in big-endian byte order.
 * LVGL native RGB565 is little-endian, so we swap bytes.
 * LCD_Init() sets 0x36 register BGR bit (bit 3) for correct colors.
 */
#define LV_COLOR_DEPTH          16
#define LV_COLOR_16_SWAP        0

/* ============================
 * Font Configuration
 * ============================ */

#define LV_FONT_MONTSERRAT_8    1
#define LV_FONT_MONTSERRAT_10   1
#define LV_FONT_MONTSERRAT_12   1
#define LV_FONT_MONTSERRAT_14   1
#define LV_FONT_MONTSERRAT_16   1
#define LV_FONT_MONTSERRAT_18   1
#define LV_FONT_MONTSERRAT_20   1
#define LV_FONT_MONTSERRAT_22   1
#define LV_FONT_MONTSERRAT_24   1
#define LV_FONT_MONTSERRAT_26   1
#define LV_FONT_MONTSERRAT_28   1
#define LV_FONT_MONTSERRAT_30   1
#define LV_FONT_MONTSERRAT_32   1
#define LV_FONT_MONTSERRAT_34   1
#define LV_FONT_MONTSERRAT_36   1
#define LV_FONT_MONTSERRAT_38   1
#define LV_FONT_MONTSERRAT_40   1
#define LV_FONT_MONTSERRAT_42   1
#define LV_FONT_MONTSERRAT_44   1
#define LV_FONT_MONTSERRAT_46   1
#define LV_FONT_MONTSERRAT_48   1

/* ============================
 * Features
 * ============================ */

#define LV_USE_LOG              0
#define LV_LOG_PRINTF           1

#define LV_USE_ASSERT_NULL      0
#define LV_USE_ASSERT_MALLOC    0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ       0
#define LV_USE_ASSERT_STYLE     0

#define LV_USE_USER_DATA        1

/* ============================
 * Input Device
 * ============================ */

#define LV_INDEV_DEF_READ_PERIOD    20

/* clang-format on */
#endif /* LV_CONF_H */
