/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ff.h"
#include "diskio.h"
#include "sd_spi.h"
#include "audio_config.h"
#include "audio_synth.h"
#include "wav_player.h"
#include "playlist.h"
#include "ili9341.h"
#include "gui_manager.h"
#include "touch_4wire.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define LCD_WIDTH               320
#define LCD_HEIGHT              240
#define LCD_FB_BYTE_PER_PIXEL   2
#define LCD_VIRTUAL_BUF_SIZE    (LCD_WIDTH * 40)
#define GUI_ONLY_MODE           0U
#define GUI_ONLY_SW_TICK_MS     5U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

COM_InitTypeDef BspCOMInit;

SPI_HandleTypeDef hspi1;
ADC_HandleTypeDef hadc1;
SAI_HandleTypeDef hsai_BlockA1;
DMA_HandleTypeDef hdma_sai1_a;
TIM_HandleTypeDef htim6;

/* USER CODE BEGIN PV */
static FATFS g_fs;
static uint8_t g_sd_test_done = 0;
static int16_t audio_buffer[AUDIO_FRAMES * 2U];
static uint8_t g_wav_mode = 0U;  /* 0=melody, 1=WAV */
static uint8_t g_audio_paused = 0U;
static float g_audio_phase = 0.0f;
static float g_audio_phase_inc = 0.0f;
static volatile uint16_t g_volume_percent = 10U;
static uint8_t g_shuffle_enabled = 0U;
static uint8_t g_viz_levels[14] = {0};
static uint32_t g_viz_lfsr = 0xA5A5F00DU;
static uint8_t Viz_NextRand(uint8_t min_v, uint8_t max_v)
{
  g_viz_lfsr ^= g_viz_lfsr << 13;
  g_viz_lfsr ^= g_viz_lfsr >> 17;
  g_viz_lfsr ^= g_viz_lfsr << 5;
  uint8_t span = (uint8_t)(max_v - min_v + 1U);
  return (uint8_t)(min_v + (g_viz_lfsr % span));
}

static void Viz_UpdateSyntheticLevels(void)
{
  if (g_wav_mode == 0U) {
    for (uint8_t i = 0; i < 14U; i++) {
      g_viz_levels[i] = Viz_NextRand(1U, 3U);
    }
  } else {
    for (uint8_t i = 0; i < 14U; i++) {
      g_viz_levels[i] = Viz_NextRand(2U, 6U);
    }
  }
  GuiManager_SetSpectrumLevels(g_viz_levels, 14U);
}

static void Viz_UpdateFromAudioHalf(uint32_t base)
{
  (void)base;
}

static void Viz_UpdateFromAudioFull(uint32_t base)
{
  (void)base;
}

/* Align to 32 bytes for D-Cache / DMA coherency */
static volatile uint32_t s_flush_count = 0U;
static uint8_t s_lcd_heartbeat_done = 0U;
static volatile uint8_t g_bl_pct = 100U;
static volatile uint8_t g_bl_tick_100 = 0U;
static volatile uint8_t g_bl_div4 = 0U;
volatile uint32_t g_mcp_test_var = 0x1234ABCDU;
static touch_calibration_t g_touch_cal = {
  .x_min = 300,
  .x_max = 3800,
  .y_min = 300,
  .y_max = 3800,
  .invert_x = false,
  .invert_y = false,
  .swap_xy = false,
};
static uint8_t g_touch_calibrating = 0U;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_SPI1_Init(void);
static void MX_ADC1_Init(void);
static void MX_SAI1_Init(void);
static void MX_TIM6_Init(void);
/* USER CODE BEGIN PFP */
static void AudioApp_FillFrames(uint16_t start_frame, uint16_t frame_count);
static void LCD_Init(void);
static void SPI1_SendBlocking(const uint8_t *data, uint32_t len);
static void LCD_HeartbeatFill(uint16_t color);
static void TouchCal_ApplyDefaults(void);
static void RefreshPlaylistFromSD(void);

void LCD_WriteCmd(uint8_t cmd);
void LCD_WriteData(uint8_t data);
void LCD_WriteMultiData(uint8_t *data, size_t length);

void HAL_SAI_TxHalfCpltCallback(SAI_HandleTypeDef *hsai);
void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static void AudioApp_FillFrames(uint16_t start_frame, uint16_t frame_count)
{
  if (g_audio_paused) {
    for (uint16_t i = 0; i < frame_count; i++) {
      uint16_t frame = (uint16_t)(start_frame + i);
      audio_buffer[frame * 2U] = 0;
      audio_buffer[frame * 2U + 1U] = 0;
    }
    return;
  }
  float volume_gain = ((float)g_volume_percent / 100.0f) * 1.8f;
#if (AUDIO_OUTPUT_MODE == AUDIO_MODE_TEST)
  for (uint16_t i = 0; i < frame_count; i++) {
    float sample = AUDIO_AMPLITUDE * sinf(g_audio_phase) * volume_gain;
    int16_t s = AudioSynth_Sat16((int32_t)(sample * 32767.0f));
    uint16_t frame = (uint16_t)(start_frame + i);
    audio_buffer[frame * 2U] = s;
    audio_buffer[frame * 2U + 1U] = s;
    g_audio_phase += g_audio_phase_inc;
    if (g_audio_phase >= (2.0f * AUDIO_PI)) {
      g_audio_phase -= (2.0f * AUDIO_PI);
    }
  }
#else
  if (g_wav_mode == 0U) {
    AudioSynth_FillFrames(audio_buffer, start_frame, frame_count);
    for (uint16_t i = 0; i < frame_count; i++) {
      uint16_t frame = (uint16_t)(start_frame + i);
      audio_buffer[frame * 2U] = AudioSynth_Sat16((int32_t)((float)audio_buffer[frame * 2U] * volume_gain));
      audio_buffer[frame * 2U + 1U] = AudioSynth_Sat16((int32_t)((float)audio_buffer[frame * 2U + 1U] * volume_gain));
    }
  } else {
    if (WavPlayer_IsFinished()) {
      if (!Playlist_PlayNext()) {
        g_wav_mode = 0U;
        AudioSynth_StartMelody();
        AudioSynth_FillFrames(audio_buffer, start_frame, frame_count);
        for (uint16_t i = 0; i < frame_count; i++) {
          uint16_t frame = (uint16_t)(start_frame + i);
          audio_buffer[frame * 2U] = AudioSynth_Sat16((int32_t)((float)audio_buffer[frame * 2U] * volume_gain));
          audio_buffer[frame * 2U + 1U] = AudioSynth_Sat16((int32_t)((float)audio_buffer[frame * 2U + 1U] * volume_gain));
        }
        return;
      }
    }
    WavPlayer_FillFrames(audio_buffer, start_frame, frame_count, volume_gain);
  }
#endif
}

void LCD_WriteCmd_NoCS(uint8_t cmd)
{
  HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_RESET);
  SPI1_SendBlocking(&cmd, 1);
}

void LCD_WriteMultiData_NoCS(const uint8_t *data, size_t length)
{
  HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET);
  SPI1_SendBlocking(data, (uint32_t)length);
}

void LCD_SetAddressWindow(int32_t x1, int32_t y1, int32_t x2, int32_t y2)
{
  uint8_t data[4];

  LCD_WriteCmd(ILI9341_CMD_COLADDR);
  data[0] = (uint8_t)((x1 >> 8) & 0xFF);
  data[1] = (uint8_t)(x1 & 0xFF);
  data[2] = (uint8_t)((x2 >> 8) & 0xFF);
  data[3] = (uint8_t)(x2 & 0xFF);
  LCD_WriteMultiData(data, 4);

  LCD_WriteCmd(ILI9341_CMD_PAGEADDR);
  data[0] = (uint8_t)((y1 >> 8) & 0xFF);
  data[1] = (uint8_t)(y1 & 0xFF);
  data[2] = (uint8_t)((y2 >> 8) & 0xFF);
  data[3] = (uint8_t)(y2 & 0xFF);
  LCD_WriteMultiData(data, 4);

  LCD_WriteCmd(ILI9341_CMD_GRAM);
  HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET);
}

void LCD_WritePixelsBlocking(const uint8_t *data, uint32_t len)
{
  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);
  SPI1_SendBlocking(data, len);
  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
  /* Not used in blocking mode */
}

static void SPI1_SendBlocking(const uint8_t *data, uint32_t len);
static void LCD_HeartbeatFill(uint16_t color);
static void TouchCal_ApplyDefaults(void)
{
  /* Precise measured points:
     TL(0,0):   x=440,  y=250
     TR(320,0): x=410,  y=2750
     BL(0,240): x=3795, y=215
     BR(320,240):x=3815,y=2740
  */
  g_touch_cal.x_min = 215;
  g_touch_cal.x_max = 2750;
  g_touch_cal.y_min = 410;
  g_touch_cal.y_max = 3815;
  g_touch_cal.invert_x = false;
  g_touch_cal.invert_y = false;
  g_touch_cal.swap_xy = true;
  printf("CAL: Applied precise measured defaults\r\n");
}

static void LCD_Init(void)
{
  /* Ensure SPI is at a safe speed for init */
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  HAL_SPI_Init(&hspi1);

  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);

  /* Hardware reset if possible, otherwise software reset */
  LCD_WriteCmd(ILI9341_CMD_RST);
  HAL_Delay(120);

  ILI9341_Init(LCD_WriteData, LCD_WriteCmd);

  LCD_WriteCmd(ILI9341_CMD_MAC);
  LCD_WriteData(0x28); /* Landscape */

  LCD_WriteCmd(ILI9341_CMD_PIXELFORMAT);
  LCD_WriteData(0x55); /* 16-bit */

  LCD_WriteCmd(ILI9341_CMD_SLEEPOUT);
  HAL_Delay(120);

  LCD_WriteCmd(ILI9341_CMD_DISPLAYON);
  HAL_Delay(20);


  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
}

static void LCD_HeartbeatFill(uint16_t color)
{
  uint8_t data[4];
  LCD_WriteCmd(ILI9341_CMD_COLADDR);
  data[0] = 0; data[1] = 0; data[2] = (LCD_WIDTH - 1) >> 8; data[3] = (LCD_WIDTH - 1) & 0xFF;
  LCD_WriteMultiData(data, 4);

  LCD_WriteCmd(ILI9341_CMD_PAGEADDR);
  data[0] = 0; data[1] = 0; data[2] = (LCD_HEIGHT - 1) >> 8; data[3] = (LCD_HEIGHT - 1) & 0xFF;
  LCD_WriteMultiData(data, 4);

  LCD_WriteCmd(ILI9341_CMD_GRAM);
  HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);

  uint8_t px[2] = {(uint8_t)(color >> 8), (uint8_t)(color & 0xFF)};
  for (uint32_t i = 0; i < (LCD_WIDTH * LCD_HEIGHT); i++) {
    SPI1_SendBlocking(px, 2);
  }

  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
}

static void RefreshPlaylistFromSD(void)
{
  if (!Playlist_Init()) {
    return;
  }

  GuiManager_ClearPlaylist();
  uint16_t count = Playlist_GetCount();
  for (uint16_t i = 0; i < count; i++) {
    GuiManager_AddTrackToList(Playlist_GetNameByIndex(i));
  }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM6) {
    g_bl_div4 = (uint8_t)((g_bl_div4 + 1U) & 0x03U);
    if (g_bl_div4 == 0U) {
      g_bl_tick_100 = (uint8_t)((g_bl_tick_100 + 1U) % 100U);
      HAL_GPIO_WritePin(GPIOG, GPIO_PIN_12, (g_bl_tick_100 < g_bl_pct) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
  }
}

/* LCD Low-level Support */
static void SPI1_SendBlocking(const uint8_t *data, uint32_t len)
{
  HAL_SPI_Transmit(&hspi1, (uint8_t *)data, (uint16_t)len, 1000);
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
  SPI1_SendBlocking(data, length);
}

/* UI Callbacks */
void GuiManager_OnPlay(void)
{
  g_audio_paused = 0U;
  g_wav_mode = 1U;
  GuiManager_SetPlaying(1U);
  printf("UI: Play\r\n");
}

void GuiManager_OnPause(void)
{
  g_audio_paused = 1U;
  GuiManager_SetPlaying(0U);
  printf("UI: Pause\r\n");
}

void GuiManager_OnStop(void)
{
  g_audio_paused = 1U;
  g_wav_mode = 0U;
  GuiManager_SetPlaying(0U);
  printf("UI: Stop\r\n");
}

void GuiManager_OnShuffleToggle(void)
{
  g_shuffle_enabled = g_shuffle_enabled ? 0U : 1U;
  GuiManager_SetShuffle(g_shuffle_enabled);
  printf("UI: Shuffle %s\r\n", g_shuffle_enabled ? "ON" : "OFF");
}

void GuiManager_OnNext(void)
{
  uint8_t ok = 0U;
  if (g_shuffle_enabled && Playlist_GetCount() > 0U) {
    uint16_t count = Playlist_GetCount();
    uint16_t start = (uint16_t)(rand() % count);
    for (uint16_t i = 0; i < count; i++) {
      uint16_t idx = (uint16_t)((start + i) % count);
      const char *name = Playlist_GetNameByIndex(idx);
      if (name != NULL && Playlist_PlaySpecific(name)) {
        ok = 1U;
        break;
      }
    }
  } else {
    ok = Playlist_PlayNextStrict();
  }

  if (ok) {
    g_audio_paused = 0U;
    g_wav_mode = 1U;
    GuiManager_SetTrackName(WavPlayer_GetCurrentName());
    GuiManager_SetCurrentTrackByName(WavPlayer_GetCurrentName());
    GuiManager_SetPlaying(1U);
  }
  printf("UI: Next\r\n");
}

void GuiManager_OnPrev(void)
{
  uint8_t ok = 0U;
  if (g_shuffle_enabled && Playlist_GetCount() > 0U) {
    uint16_t count = Playlist_GetCount();
    uint16_t start = (uint16_t)(rand() % count);
    for (uint16_t i = 0; i < count; i++) {
      uint16_t idx = (uint16_t)((start + i) % count);
      const char *name = Playlist_GetNameByIndex(idx);
      if (name != NULL && Playlist_PlaySpecific(name)) {
        ok = 1U;
        break;
      }
    }
  } else {
    ok = Playlist_PlayPrevStrict();
  }

  if (ok) {
    g_audio_paused = 0U;
    g_wav_mode = 1U;
    GuiManager_SetTrackName(WavPlayer_GetCurrentName());
    GuiManager_SetCurrentTrackByName(WavPlayer_GetCurrentName());
    GuiManager_SetPlaying(1U);
  }
  printf("UI: Prev\r\n");
}

void GuiManager_OnTrackSelect(const char *name)
{
  static uint32_t last_select_ms = 0U;
  if (name == NULL) {
    return;
  }

  uint32_t now = HAL_GetTick();
  if ((now - last_select_ms) < 250U) {
    return;
  }
  last_select_ms = now;

  const char *cur = WavPlayer_GetCurrentName();
  if (cur != NULL && cur[0] != 0 && strcmp(cur, name) == 0) {
    return;
  }

  if (Playlist_PlaySpecific(name)) {
    g_wav_mode = 1U;
    GuiManager_SetTrackName(WavPlayer_GetCurrentName());
    GuiManager_SetCurrentTrackByName(WavPlayer_GetCurrentName());
    GuiManager_SetPlaying(1U);
    printf("UI: Selected %s\r\n", name);
  }
}

void GuiManager_OnRefreshPlaylist(void)
{
  if (!g_sd_test_done) {
    return;
  }
  RefreshPlaylistFromSD();
  printf("UI: Playlist refreshed\r\n");
}

void GuiManager_OnBrightnessChange(uint8_t level)
{
  static const uint8_t lut[5] = {20U, 40U, 60U, 80U, 100U};
  if (level > 4U) level = 4U;
  g_bl_pct = lut[level];
}

void GuiManager_OnVolumeChange(uint16_t volume)
{
  if (volume > 100U) {
    volume = 100U;
  }
  g_volume_percent = volume;
}

void HAL_SAI_TxHalfCpltCallback(SAI_HandleTypeDef *hsai)
{
  if (hsai->Instance == SAI1_Block_A) {
    AudioApp_FillFrames(0U, AUDIO_FRAMES / 2U);
    Viz_UpdateFromAudioHalf(0U);
    SCB_CleanDCache_by_Addr((uint32_t*)&audio_buffer[0], AUDIO_FRAMES * sizeof(int16_t));
  }
}

void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai)
{
  if (hsai->Instance == SAI1_Block_A) {
    AudioApp_FillFrames(AUDIO_FRAMES / 2U, AUDIO_FRAMES / 2U);
    Viz_UpdateFromAudioFull(AUDIO_FRAMES);
    SCB_CleanDCache_by_Addr((uint32_t*)&audio_buffer[AUDIO_FRAMES], AUDIO_FRAMES * sizeof(int16_t));
  }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  SCB_EnableICache();
  SCB_EnableDCache();
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  setvbuf(stdout, NULL, _IONBF, 0);
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize COM1 port (115200, 8 bits (7-bit data + 1 stop bit), no parity */
  BspCOMInit.BaudRate   = 115200;
  BspCOMInit.WordLength = COM_WORDLENGTH_8B;
  BspCOMInit.StopBits   = COM_STOPBITS_1;
  BspCOMInit.Parity     = COM_PARITY_NONE;
  BspCOMInit.HwFlowCtl  = COM_HWCONTROL_NONE;
  if (BSP_COM_Init(COM1, &BspCOMInit) != BSP_ERROR_NONE)
  {
    Error_Handler();
  }

  /* Direct UART Sanity Check */
  HAL_UART_Transmit(&hcom_uart[COM1], (uint8_t *)"COM_OK\r\n", 8, 100);

  /* Initialize all configured peripherals */
  HAL_UART_Transmit(&hcom_uart[COM1], (uint8_t *)"MX_GPIO...\r\n", 12, 100);
  MX_GPIO_Init();
  HAL_UART_Transmit(&hcom_uart[COM1], (uint8_t *)"MX_SPI...\r\n", 11, 100);
  MX_SPI1_Init();
#ifdef HAL_ADC_MODULE_ENABLED
  HAL_UART_Transmit(&hcom_uart[COM1], (uint8_t *)"MX_ADC...\r\n", 11, 100);
  MX_ADC1_Init();
#endif
  HAL_UART_Transmit(&hcom_uart[COM1], (uint8_t *)"MX_SAI...\r\n", 11, 100);
  MX_SAI1_Init();
  HAL_UART_Transmit(&hcom_uart[COM1], (uint8_t *)"MX_TIM...\r\n", 11, 100);
  MX_TIM6_Init();
  HAL_UART_Transmit(&hcom_uart[COM1], (uint8_t *)"MX_DONE\r\n", 9, 100);
  /* USER CODE BEGIN 2 */
  HAL_TIM_Base_Start_IT(&htim6);

  LCD_Init();
  Touch4W_Init(&hadc1);

  TouchCal_ApplyDefaults();
  GuiManager_Init();

  printf("Clock+COM up\r\n");
  HAL_Delay(20);
  printf("Peripherals up\r\n");
  printf("MCP Lifecycle Hook: build/flash OK\r\n");
  HAL_Delay(20);

  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_RESET);

  s_lcd_heartbeat_done = 1U;

  printf("GUI Up\r\n");

#if defined(GUI_ONLY_MODE) && (GUI_ONLY_MODE == 1U)
  printf("GUI_ONLY_MODE: Bypassing SD and Audio init\r\n");
  goto main_loop;
#endif

  /* USER CODE END 2 */

  /* USER CODE BEGIN 3 */
  printf("\r\n--- STM32H7 Music Player ---\r\n");

  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_14, GPIO_PIN_SET);
  HAL_Delay(20);

  /* Force slow SPI for SD init */
  hspi1.Instance->CFG1 &= ~SPI_CFG1_MBR;
  hspi1.Instance->CFG1 |= SPI_BAUDRATEPRESCALER_64;

  printf("before SD init\r\n");
  HAL_Delay(10);

  /* Initialize SD card */
  if (SD_IO_Init()) {
    printf("SD Init: OK\r\n");
  } else {
    printf("SD Init: FAILED\r\n");
  }

  printf("after SD init\r\n");
  HAL_Delay(10);

  /* Restore SPI to ~20MHz for LCD transfers */
  hspi1.Instance->CFG1 &= ~SPI_CFG1_MBR;
  hspi1.Instance->CFG1 |= SPI_BAUDRATEPRESCALER_16;

  /* Mount Filesystem */
  {
    FRESULT fr = f_mount(&g_fs, "", 1);
    if (fr == FR_OK) {
      printf("Mount: OK\r\n");
      g_sd_test_done = 1;
    } else {
      printf("Mount: FAILED (FR=%d)\r\n", (int)fr);
    }
  }

  printf("after mount\r\n");
  HAL_Delay(10);

  /* Audio App Setup */
  AudioSynth_Init();

#if (AUDIO_OUTPUT_MODE == AUDIO_MODE_TEST)
  g_wav_mode = 0U;
  g_audio_phase = 0.0f;
  g_audio_phase_inc = (2.0f * AUDIO_PI * AUDIO_TEST_FREQ_HZ) / AUDIO_SAMPLE_RATE;
  AudioApp_FillFrames(0U, AUDIO_FRAMES);
  printf("Mode: TEST (%.0f Hz sine)\r\n", AUDIO_TEST_FREQ_HZ);
#else
  if (Playlist_Init()) {
    printf("Scanning SD card...\r\n");
    RefreshPlaylistFromSD();
    printf("Playlist count: %u\r\n", (unsigned)Playlist_GetCount());

    if (Playlist_PlayNext()) {
      g_wav_mode = 1U;
    }

    if (g_wav_mode == 1U) {
      GuiManager_SetTrackName(WavPlayer_GetCurrentName());
      GuiManager_SetCurrentTrackByName(WavPlayer_GetCurrentName());
      GuiManager_SetPlaying(1U);
    }
  }

  if (g_wav_mode == 0U) {
    GuiManager_SetPlaying(0U);
    printf("Fallback: MELODY\r\n");
    AudioSynth_StartMelody();
  }
  AudioApp_FillFrames(0U, AUDIO_FRAMES);
#endif

  printf("before SAI DMA start\r\n");
  HAL_Delay(10);

  /* Start DMA Transmit */
  if (HAL_SAI_Transmit_DMA(&hsai_BlockA1, (uint8_t *)audio_buffer, AUDIO_FRAMES * 2U) != HAL_OK) {
    printf("SAI DMA: FAILED\r\n");
    Error_Handler();
  }

  printf("Entering main loop\r\n");
  HAL_Delay(10);

main_loop:
  while (1)
  {
    static uint32_t last_gui_update = 0;
    static uint32_t last_viz_update = 0;
    uint32_t now = HAL_GetTick();

    uint16_t tx, ty;
    if (Touch4W_GetCalibratedXY(&tx, &ty, &g_touch_cal, LCD_WIDTH, LCD_HEIGHT)) {
        GuiManager_HandleTouch(tx, ty, 1);
    } else {
        GuiManager_HandleTouch(0, 0, 0);
    }

    if (g_touch_calibrating == 0U) {
      if ((now - last_viz_update) >= 120U) {
        last_viz_update = now;
        if (!g_audio_paused) {
          Viz_UpdateSyntheticLevels();
        } else {
          for (uint8_t i = 0; i < 14U; i++) {
            g_viz_levels[i] = 0U;
          }
          GuiManager_SetSpectrumLevels(g_viz_levels, 14U);
        }
        GuiManager_RenderSpectrumOnly();
      }
      if ((now - last_gui_update) >= 180U) {
        last_gui_update = now;
        uint32_t elapsed = WavPlayer_GetElapsedSeconds();
        GuiManager_SetPlaybackTime((uint16_t)(elapsed / 60U), (uint16_t)(elapsed % 60U));
        GuiManager_Update();
      }
    }

    WavPlayer_Pump();
    HAL_Delay(5);
  }
  /* USER CODE END 3 */

  /* USER CODE END 3 */
}

/**
  * @brief TIM6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM6_Init(void)
{
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  htim6.Instance = TIM6;
  /* 240MHz / 24 = 10MHz -> 100 period = 10us tick (100kHz ISR) */
  htim6.Init.Prescaler = 23;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = 99;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE0) != HAL_OK)
  {
    Error_Handler();
  }
  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 60;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SAI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SAI1_Init(void)
{

  hsai_BlockA1.Instance = SAI1_Block_A;
  hsai_BlockA1.Init.AudioMode = SAI_MODEMASTER_TX;
  hsai_BlockA1.Init.Synchro = SAI_ASYNCHRONOUS;
  hsai_BlockA1.Init.OutputDrive = SAI_OUTPUTDRIVE_ENABLE;
  hsai_BlockA1.Init.NoDivider = SAI_MASTERDIVIDER_ENABLE;
  hsai_BlockA1.Init.FIFOThreshold = SAI_FIFOTHRESHOLD_EMPTY;
  hsai_BlockA1.Init.AudioFrequency = SAI_AUDIO_FREQUENCY_16K;
  hsai_BlockA1.Init.SynchroExt = SAI_SYNCEXT_DISABLE;
  hsai_BlockA1.Init.MonoStereoMode = SAI_STEREOMODE;
  hsai_BlockA1.Init.CompandingMode = SAI_NOCOMPANDING;
  hsai_BlockA1.Init.TriState = SAI_OUTPUT_NOTRELEASED;
  hsai_BlockA1.Init.MckOverSampling = SAI_MCK_OVERSAMPLING_DISABLE;

  if (HAL_SAI_InitProtocol(&hsai_BlockA1, SAI_I2S_STANDARD, SAI_PROTOCOL_DATASIZE_16BIT, 2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
#ifdef HAL_ADC_MODULE_ENABLED
static void MX_ADC1_Init(void)
{
  ADC_MultiModeTypeDef multimode = {0};

  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DR;
  hadc1.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  hadc1.Init.LeftBitShift = ADC_LEFTBITSHIFT_NONE;
  hadc1.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  multimode.Mode = ADC_MODE_INDEPENDENT;
  if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode) != HAL_OK)
  {
    Error_Handler();
  }
}
#endif

static void MX_SPI1_Init(void)
{
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 0x0;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  hspi1.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
  hspi1.Init.FifoThreshold = SPI_FIFO_THRESHOLD_08DATA;
  hspi1.Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi1.Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi1.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
  hspi1.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
  hspi1.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_ENABLE;
  hspi1.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_ENABLE;
  hspi1.Init.IOSwap = SPI_IO_SWAP_DISABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOG, GPIO_PIN_12, GPIO_PIN_SET);

  /* Set initial states: SD CS high, LCD CS high, LCD DC low */
  HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_RESET);

  /* SD_CS(PE14) */
  GPIO_InitStruct.Pin = SD_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(SD_CS_GPIO_Port, &GPIO_InitStruct);

  /* LCD_CS(PD14) */
  GPIO_InitStruct.Pin = LCD_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(LCD_CS_GPIO_Port, &GPIO_InitStruct);

  /* LCD_DC(PE9) */
  GPIO_InitStruct.Pin = LCD_DC_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(LCD_DC_GPIO_Port, &GPIO_InitStruct);

  /* LCD_BL(PG12) */
  GPIO_InitStruct.Pin = GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}
