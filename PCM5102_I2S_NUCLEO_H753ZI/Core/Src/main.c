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
#include <math.h>
#include "ff.h"
#include "diskio.h"
#include "sd_spi.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

COM_InitTypeDef BspCOMInit;

SPI_HandleTypeDef hspi1;
SAI_HandleTypeDef hsai_BlockA1;
DMA_HandleTypeDef hdma_sai1_a;

/* USER CODE BEGIN PV */
static FATFS g_fs;
static uint8_t g_sd_test_done = 0;

#define AUDIO_FRAMES            512U
#define AUDIO_SAMPLE_RATE       16000.0f
#define AUDIO_AMPLITUDE         0.01f
#define AUDIO_GAIN              5.0f//32.0f
#define AUDIO_PI                3.14159265358979323846f
#define WAV_AMPLITUDE           (AUDIO_AMPLITUDE * AUDIO_GAIN)

#define AUDIO_MODE_WAV_OR_MELODY 0U
#define AUDIO_MODE_TEST     	 1U
#define AUDIO_OUTPUT_MODE        AUDIO_MODE_WAV_OR_MELODY
#define AUDIO_TEST_FREQ_HZ       500.0f

#if (AUDIO_OUTPUT_MODE != AUDIO_MODE_TEST)
typedef struct {
  float freq_hz;
  uint32_t duration_frames;
} audio_note_t;

static const audio_note_t g_melody[] = {
  {261.63f, 16000U},
  {293.66f, 16000U},
  {329.63f, 16000U},
  {349.23f, 16000U},
  {392.00f, 16000U},
  {440.00f, 16000U},
  {493.88f, 16000U},
  {523.25f, 16000U},
  {493.88f, 16000U},
  {440.00f, 16000U},
  {392.00f, 16000U},
  {349.23f, 16000U},
  {329.63f, 16000U},
  {293.66f, 16000U},
  {261.63f, 16000U},
};
#endif

static int16_t audio_buffer[AUDIO_FRAMES * 2U];
static float audio_phase = 0.0f;
static float audio_phase_inc = 0.0f;
#if (AUDIO_OUTPUT_MODE != AUDIO_MODE_TEST)
static uint32_t melody_note_index = 0U;
static uint32_t melody_note_frames_left = 0U;
#endif

#if (AUDIO_OUTPUT_MODE != AUDIO_MODE_TEST)
/* WAV playback variables */
static FIL g_wav_file;
static uint32_t g_wav_data_size = 0U;
static uint32_t g_wav_bytes_played = 0U;
static uint8_t g_wav_mono = 0U;
static uint32_t g_wav_sample_rate = 0U;
static char g_wav_current_name[13];
static DIR g_wav_dir;
#endif
static uint8_t g_wav_mode = 0U;  /* 0=melody, 1=WAV */
#if (AUDIO_OUTPUT_MODE != AUDIO_MODE_TEST)
static uint8_t g_wav_dump = 1U;  /* dump first few samples once */
#endif
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_SAI1_Init(void);
/* USER CODE BEGIN PFP */
static void Audio_PlayNote(float freq_hz);
static void Audio_FillFrames(uint16_t start_frame, uint16_t frame_count);
#if (AUDIO_OUTPUT_MODE != AUDIO_MODE_TEST)
static uint32_t WAV_ReadLE32(const uint8_t *p);
static uint16_t WAV_ReadLE16(const uint8_t *p);
static uint8_t WAV_HasExt(const char *name);
static uint8_t WAV_OpenAndValidate(const char *name);
static uint8_t WAV_PlayNext(void);
static void WAV_FillFrames(uint16_t start_frame, uint16_t frame_count);
#endif
void HAL_SAI_TxHalfCpltCallback(SAI_HandleTypeDef *hsai);
void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static int16_t Audio_Sat16(int32_t v)
{
  if (v > 32767) return 32767;
  if (v < -32768) return -32768;
  return (int16_t)v;
}

static void Audio_PlayNote(float freq_hz)
{
  audio_phase = 0.0f;
  audio_phase_inc = (2.0f * AUDIO_PI * freq_hz) / AUDIO_SAMPLE_RATE;
}

static void Audio_FillFrames(uint16_t start_frame, uint16_t frame_count)
{
  for (uint16_t i = 0; i < frame_count; i++) {
#if (AUDIO_OUTPUT_MODE != AUDIO_MODE_TEST)
    if (melody_note_frames_left == 0U) {
      melody_note_index++;
      if (melody_note_index >= (sizeof(g_melody) / sizeof(g_melody[0]))) {
        melody_note_index = 0U;
      }
      Audio_PlayNote(g_melody[melody_note_index].freq_hz);
      melody_note_frames_left = g_melody[melody_note_index].duration_frames;
    }
#endif

    float sample = AUDIO_AMPLITUDE * sinf(audio_phase);
    int16_t s = Audio_Sat16((int32_t)(sample * 32767.0f));
    uint16_t frame = (uint16_t)(start_frame + i);
    audio_buffer[frame * 2U] = s;
    audio_buffer[frame * 2U + 1U] = s;

    audio_phase += audio_phase_inc;
    if (audio_phase >= (2.0f * AUDIO_PI)) {
      audio_phase -= (2.0f * AUDIO_PI);
    }

#if (AUDIO_OUTPUT_MODE != AUDIO_MODE_TEST)
    melody_note_frames_left--;
#endif
  }
}

#if (AUDIO_OUTPUT_MODE != AUDIO_MODE_TEST)
static uint32_t WAV_ReadLE32(const uint8_t *p)
{
  return ((uint32_t)p[0])
       | ((uint32_t)p[1] << 8)
       | ((uint32_t)p[2] << 16)
       | ((uint32_t)p[3] << 24);
}

static uint16_t WAV_ReadLE16(const uint8_t *p)
{
  return (uint16_t)(((uint16_t)p[0]) | ((uint16_t)p[1] << 8));
}

static uint8_t WAV_HasExt(const char *name)
{
  uint32_t len = 0U;
  while (name[len] != 0) {
    len++;
  }
  if (len < 4U) {
    return 0U;
  }

  char c0 = name[len - 4U];
  char c1 = name[len - 3U];
  char c2 = name[len - 2U];
  char c3 = name[len - 1U];

  if (c0 != '.') {
    return 0U;
  }

  if ((c1 == 'w' || c1 == 'W') &&
      (c2 == 'a' || c2 == 'A') &&
      (c3 == 'v' || c3 == 'V')) {
    return 1U;
  }

  return 0U;
}

static uint8_t WAV_OpenAndValidate(const char *name)
{
  /* Read enough to cover typical metadata chunks before 'data' */
  uint8_t header[4096];
  UINT br = 0U;
  FRESULT fr = f_open(&g_wav_file, name, FA_READ);
  if (fr != FR_OK) {
    printf("WAV open failed: %s (FR=%d)\r\n", name, (int)fr);
    return 0U;
  }

  fr = f_read(&g_wav_file, header, sizeof(header), &br);
  if ((fr != FR_OK) || (br < 44U)) {
    printf("WAV header read failed: %s\r\n", name);
    f_close(&g_wav_file);
    return 0U;
  }

  /* Check RIFF/WAVE header */
  if ((header[0] != 'R') || (header[1] != 'I') || (header[2] != 'F') || (header[3] != 'F') ||
      (header[8] != 'W') || (header[9] != 'A') || (header[10] != 'V') || (header[11] != 'E')) {
    printf("WAV not RIFF/WAVE: %s\r\n", name);
    f_close(&g_wav_file);
    return 0U;
  }

  /* Scan through chunks to find fmt and data */
  uint16_t channels = 0U;
  uint32_t sample_rate = 0U;
  uint16_t bits_per_sample = 0U;
  uint32_t data_size = 0U;
  uint32_t data_offset = 0U;
  uint8_t found_fmt = 0U;
  uint32_t pos = 12U;

  while (pos + 8U < (uint32_t)br) {
    uint32_t chunk_size = WAV_ReadLE32(&header[pos + 4U]);

    if (g_wav_dump) {
      printf("CHK %.4s @%lu size=%lu\r\n", &header[pos], pos, chunk_size);
    }

    if ((header[pos] == 'f') && (header[pos + 1U] == 'm') &&
        (header[pos + 2U] == 't') && (header[pos + 3U] == ' ')) {
      if (chunk_size >= 16U) {
        uint16_t audio_fmt = WAV_ReadLE16(&header[pos + 8U]);
        channels = WAV_ReadLE16(&header[pos + 10U]);
        sample_rate = WAV_ReadLE32(&header[pos + 12U]);
        bits_per_sample = WAV_ReadLE16(&header[pos + 22U]);
        if (audio_fmt == 1U) {
          found_fmt = 1U;
        }
      }
    } else if ((header[pos] == 'd') && (header[pos + 1U] == 'a') &&
               (header[pos + 2U] == 't') && (header[pos + 3U] == 'a')) {
      data_size = chunk_size;
      data_offset = pos + 8U;
      break;
    }

    /* Advance to next chunk (word-aligned) */
    pos += 8U + chunk_size;
    if (chunk_size % 2U) {
      pos += 1U;
    }
  }

  if (!found_fmt) {
    printf("WAV no fmt chunk: %s\r\n", name);
    f_close(&g_wav_file);
    return 0U;
  }

  if (data_size == 0U) {
    printf("WAV data beyond 4KB header: %s\r\n", name);
    /* Seek to file end - 44 to find last chunk (data usually last) */
    FSIZE_t fsize = f_size(&g_wav_file);
    if (fsize > 44U) {
      if (f_lseek(&g_wav_file, fsize - 44U) == FR_OK) {
        UINT r = 0U;
        uint8_t tail[44];
        if (f_read(&g_wav_file, tail, sizeof(tail), &r) == FR_OK && r == 44U) {
          if ((tail[0] == 'd') && (tail[1] == 'a') &&
              (tail[2] == 't') && (tail[3] == 'a')) {
            data_size = WAV_ReadLE32(&tail[4]);
            data_offset = (uint32_t)(fsize - 44U + 8U);
            printf("WAV found data at EOF-44: %lu bytes\r\n", data_size);
          }
        }
      }
    }
    if (data_size == 0U) {
      f_close(&g_wav_file);
      return 0U;
    }
  }

  if ((sample_rate != 16000U) ||
      (bits_per_sample != 16U) ||
      ((channels != 1U) && (channels != 2U))) {
    printf("WAV spec mismatch: %s (sr=%lu, bps=%u, ch=%u)\r\n",
           name, sample_rate, bits_per_sample, channels);
    f_close(&g_wav_file);
    return 0U;
  }

  /* Seek to start of audio data */
  if (f_lseek(&g_wav_file, data_offset) != FR_OK) {
    printf("WAV seek failed: %s\r\n", name);
    f_close(&g_wav_file);
    return 0U;
  }

  g_wav_data_size = data_size;
  g_wav_bytes_played = 0U;
  g_wav_mono = (channels == 1U) ? 1U : 0U;
  g_wav_sample_rate = sample_rate;

  uint32_t ni = 0U;
  while ((name[ni] != 0) && (ni < (sizeof(g_wav_current_name) - 1U))) {
    g_wav_current_name[ni] = name[ni];
    ni++;
  }
  g_wav_current_name[ni] = 0;

  printf("Playing WAV: %s (sr=%lu, ch=%u, data_ofs=%lu)\r\n",
         name, sample_rate, channels, data_offset);
  g_wav_dump = 0U;
  return 1U;
}

static uint8_t WAV_PlayNext(void)
{
  FILINFO fno;
  while (f_readdir(&g_wav_dir, &fno) == FR_OK) {
    if (fno.fname[0] == 0) break;
    if (!(fno.fattrib & AM_DIR) && WAV_HasExt(fno.fname)) {
      /* Skip if same as current */
      uint8_t match = 1U;
      for (uint32_t i = 0; i < 13; i++) {
        if (fno.fname[i] != g_wav_current_name[i]) {
          match = 0U;
          break;
        }
        if (fno.fname[i] == 0) break;
      }
      if (match) continue;

      if (WAV_OpenAndValidate(fno.fname)) {
        return 1U;
      }
    }
  }
  /* End of directory, wrap around */
  f_closedir(&g_wav_dir);
  if (f_opendir(&g_wav_dir, "/") == FR_OK) {
    while (f_readdir(&g_wav_dir, &fno) == FR_OK) {
      if (fno.fname[0] == 0) break;
      if (!(fno.fattrib & AM_DIR) && WAV_HasExt(fno.fname)) {
        if (WAV_OpenAndValidate(fno.fname)) {
          return 1U;
        }
      }
    }
  }
  return 0U;
}

static void WAV_FillFrames(uint16_t start_frame, uint16_t frame_count)
{
  static uint8_t io_buf[AUDIO_FRAMES * 4U];

  if (g_wav_mode == 0U) {
    Audio_FillFrames(start_frame, frame_count);
    return;
  }

  if (g_wav_bytes_played >= g_wav_data_size) {
    if (!WAV_PlayNext()) {
      for (uint16_t i = 0; i < frame_count; i++) {
        uint16_t frame = (uint16_t)(start_frame + i);
        audio_buffer[frame * 2U] = 0;
        audio_buffer[frame * 2U + 1U] = 0;
      }
      return;
    }
  }

  /* Since hardware is now 16k, 16k WAV is a direct 1:1 copy */
  if (g_wav_sample_rate == 16000U) {
    uint32_t bytes_per_frame = g_wav_mono ? 2U : 4U;
    uint32_t req_bytes = (uint32_t)frame_count * bytes_per_frame;
    uint32_t left = g_wav_data_size - g_wav_bytes_played;
    if (req_bytes > left) req_bytes = left;

    UINT br = 0U;
    if (req_bytes > 0U) {
      f_read(&g_wav_file, io_buf, req_bytes, &br);
    }
    g_wav_bytes_played += br;

    uint32_t got_frames = br / bytes_per_frame;
    for (uint16_t i = 0; i < frame_count; i++) {
      uint16_t frame = (uint16_t)(start_frame + i);
      if ((uint32_t)i < got_frames) {
        int16_t l, r;
        if (g_wav_mono) {
          l = (int16_t)((uint16_t)io_buf[i * 2U] | ((uint16_t)io_buf[i * 2U + 1U] << 8));
          r = l;
        } else {
          uint32_t j = (uint32_t)i * 4U;
          l = (int16_t)((uint16_t)io_buf[j] | ((uint16_t)io_buf[j + 1U] << 8));
          r = (int16_t)((uint16_t)io_buf[j + 2U] | ((uint16_t)io_buf[j + 3U] << 8));
        }
        audio_buffer[frame * 2U] = Audio_Sat16((int32_t)((float)l * WAV_AMPLITUDE));
        audio_buffer[frame * 2U + 1U] = Audio_Sat16((int32_t)((float)r * WAV_AMPLITUDE));
      } else {
        audio_buffer[frame * 2U] = 0;
        audio_buffer[frame * 2U + 1U] = 0;
      }
    }
    return;
  }

  if (g_wav_sample_rate == 44100U) {
    static uint8_t src_buf[AUDIO_FRAMES * 4U];
    static uint32_t src_buf_frames = 0U;
    static uint32_t src_buf_index = 0U;
    static uint32_t src_acc = 0U;
    static uint8_t src_eof = 0U;
    uint32_t bytes_per_frame = g_wav_mono ? 2U : 4U;

    if (g_wav_bytes_played == 0U) {
      src_buf_frames = 0U;
      src_buf_index = 0U;
      src_acc = 0U;
      src_eof = 0U;
    }

    for (uint16_t i = 0; i < frame_count; i++) {
      while (!src_eof && (src_buf_index >= src_buf_frames)) {
        uint32_t max_bytes = sizeof(src_buf);
        uint32_t left = g_wav_data_size - g_wav_bytes_played;
        if (left == 0U) {
          src_eof = 1U;
          break;
        }
        if (max_bytes > left) {
          max_bytes = left;
        }

        UINT br = 0U;
        if ((max_bytes > 0U) && (f_read(&g_wav_file, src_buf, max_bytes, &br) == FR_OK)) {
          g_wav_bytes_played += br;
          src_buf_frames = br / bytes_per_frame;
          src_buf_index = 0U;
          if (src_buf_frames == 0U) {
            src_eof = 1U;
          }

          if (g_wav_dump && br >= 8U) {
            int16_t l0 = (int16_t)((uint16_t)src_buf[0] | ((uint16_t)src_buf[1] << 8));
            int16_t r0 = (int16_t)((uint16_t)src_buf[2] | ((uint16_t)src_buf[3] << 8));
            int16_t l1 = (int16_t)((uint16_t)src_buf[4] | ((uint16_t)src_buf[5] << 8));
            int16_t r1 = (int16_t)((uint16_t)src_buf[6] | ((uint16_t)src_buf[7] << 8));
            printf("WAV DBG L0=%d R0=%d L1=%d R1=%d\r\n", l0, r0, l1, r1);
            g_wav_dump = 0U;
          }
        } else {
          src_eof = 1U;
        }
      }

      int16_t l = 0;
      int16_t r = 0;
      if (!src_eof && (src_buf_index < src_buf_frames)) {
        if (g_wav_mono) {
          uint32_t j = src_buf_index * 2U;
          l = (int16_t)((uint16_t)src_buf[j] | ((uint16_t)src_buf[j + 1U] << 8));
          r = l;
        } else {
          uint32_t j = src_buf_index * 4U;
          l = (int16_t)((uint16_t)src_buf[j] | ((uint16_t)src_buf[j + 1U] << 8));
          r = (int16_t)((uint16_t)src_buf[j + 2U] | ((uint16_t)src_buf[j + 3U] << 8));
        }
      }

      uint16_t frame = (uint16_t)(start_frame + i);
      audio_buffer[frame * 2U] = Audio_Sat16((int32_t)((float)l * WAV_AMPLITUDE));
      audio_buffer[frame * 2U + 1U] = Audio_Sat16((int32_t)((float)r * WAV_AMPLITUDE));

      src_acc += 147U;
      while (src_acc >= 160U) {
        src_acc -= 160U;
        src_buf_index++;
      }
    }
    return;
  }

  /* 48k direct path */
  uint32_t bytes_per_frame = g_wav_mono ? 2U : 4U;
  uint32_t req_bytes = (uint32_t)frame_count * bytes_per_frame;
  uint32_t left = g_wav_data_size - g_wav_bytes_played;
  if (req_bytes > left) {
    req_bytes = left;
  }

  UINT br = 0U;
  if (req_bytes > 0U) {
    if (f_read(&g_wav_file, io_buf, req_bytes, &br) != FR_OK) {
      br = 0U;
    }
  }
  g_wav_bytes_played += br;

  if (g_wav_dump && br >= 8U) {
    int16_t l0 = (int16_t)((uint16_t)io_buf[0] | ((uint16_t)io_buf[1] << 8));
    int16_t r0 = (int16_t)((uint16_t)io_buf[2] | ((uint16_t)io_buf[3] << 8));
    int16_t l1 = (int16_t)((uint16_t)io_buf[4] | ((uint16_t)io_buf[5] << 8));
    int16_t r1 = (int16_t)((uint16_t)io_buf[6] | ((uint16_t)io_buf[7] << 8));
    printf("WAV DBG L0=%d R0=%d L1=%d R1=%d\r\n", l0, r0, l1, r1);
    g_wav_dump = 0U;
  }

  uint32_t got_frames = br / bytes_per_frame;
  for (uint16_t i = 0; i < frame_count; i++) {
    uint16_t frame = (uint16_t)(start_frame + i);
    if ((uint32_t)i < got_frames) {
      if (g_wav_mono) {
        int16_t s = (int16_t)((uint16_t)io_buf[i * 2U] | ((uint16_t)io_buf[i * 2U + 1U] << 8));
        audio_buffer[frame * 2U] = Audio_Sat16((int32_t)((float)s * WAV_AMPLITUDE));
        audio_buffer[frame * 2U + 1U] = Audio_Sat16((int32_t)((float)s * WAV_AMPLITUDE));
      } else {
        uint32_t j = (uint32_t)i * 4U;
        int16_t l = (int16_t)((uint16_t)io_buf[j] | ((uint16_t)io_buf[j + 1U] << 8));
        int16_t r = (int16_t)((uint16_t)io_buf[j + 2U] | ((uint16_t)io_buf[j + 3U] << 8));
        audio_buffer[frame * 2U] = Audio_Sat16((int32_t)((float)l * WAV_AMPLITUDE));
        audio_buffer[frame * 2U + 1U] = Audio_Sat16((int32_t)((float)r * WAV_AMPLITUDE));
      }
    } else {
      audio_buffer[frame * 2U] = 0;
      audio_buffer[frame * 2U + 1U] = 0;
    }
  }
}
#endif

void HAL_SAI_TxHalfCpltCallback(SAI_HandleTypeDef *hsai)
{
  if (hsai->Instance == SAI1_Block_A) {
#if (AUDIO_OUTPUT_MODE == AUDIO_MODE_TEST)
    Audio_FillFrames(0U, AUDIO_FRAMES / 2U);
#else
    WAV_FillFrames(0U, AUDIO_FRAMES / 2U);
#endif
  }
}

void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai)
{
  if (hsai->Instance == SAI1_Block_A) {
#if (AUDIO_OUTPUT_MODE == AUDIO_MODE_TEST)
    Audio_FillFrames(AUDIO_FRAMES / 2U, AUDIO_FRAMES / 2U);
#else
    WAV_FillFrames(AUDIO_FRAMES / 2U, AUDIO_FRAMES / 2U);
#endif
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

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI1_Init();
  MX_SAI1_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

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

  /* USER CODE BEGIN 3 */
  printf("\r\n--- STM32H7 SD Card Test ---\r\n");

  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_14, GPIO_PIN_SET);
  HAL_Delay(20);

  /* Initialize SD card after UART is ready */
  if (SD_IO_Init()) {
    printf("SD Init: OK\r\n");
  } else {
    printf("SD Init: FAILED\r\n");
  }

  {
    FRESULT fr = f_mount(&g_fs, "", 1);
    if (fr == FR_OK) {
      printf("Mount: OK\r\n");
      g_sd_test_done = 1;
    } else if (fr == FR_NO_FILESYSTEM) {
      static uint8_t work[4096];
      printf("Mount: NO_FILESYSTEM, formatting...\r\n");
      if (f_mkfs("", FM_FAT32, 0, work, sizeof(work)) == FR_OK) {
        if (f_mount(&g_fs, "", 1) == FR_OK) {
          printf("Mount after mkfs: OK\r\n");
          g_sd_test_done = 1;
        } else {
          printf("Mount after mkfs: FAILED\r\n");
        }
      } else {
        printf("mkfs: FAILED\r\n");
      }
    } else {
      printf("Mount: FAILED (FR=%d)\r\n", (int)fr);
    }
  }

#if (AUDIO_OUTPUT_MODE == AUDIO_MODE_TEST)
  g_wav_mode = 0U;
  Audio_PlayNote(AUDIO_TEST_FREQ_HZ);
  Audio_FillFrames(0U, AUDIO_FRAMES);
  printf("Audio test mode: %.0f Hz sine\r\n", AUDIO_TEST_FREQ_HZ);
#else
  /* Try preferred WAV first, then continue directory scan */
  {
    g_wav_mode = 0U;
    printf("Scanning SD card...\r\n");
    if (f_opendir(&g_wav_dir, "/") == FR_OK) {
      printf("Try preferred WAV: OASIS-~1.WAV\r\n");
      if (WAV_OpenAndValidate("OASIS-~1.WAV")) {
        g_wav_mode = 1U;
        /* Note: directory pointer stays at start because we opened by name */
      } else {
        printf("Preferred WAV not available/valid, searching directory...\r\n");
        if (WAV_PlayNext()) {
          g_wav_mode = 1U;
        }
      }
    }

    /* Fall back to melody if no valid WAV found */
    if (g_wav_mode == 0U) {
      printf("No valid WAV found, playing melody.\r\n");
      melody_note_index = 0U;
      Audio_PlayNote(g_melody[0].freq_hz);
      melody_note_frames_left = g_melody[0].duration_frames;
      Audio_FillFrames(0U, AUDIO_FRAMES);
      melody_note_frames_left -= AUDIO_FRAMES;
      melody_note_frames_left -= AUDIO_FRAMES;
    } else {
      /* Pre-fill buffer for DMA start */
      WAV_FillFrames(0U, AUDIO_FRAMES);
    }
  }
#endif

  if (HAL_SAI_Transmit_DMA(&hsai_BlockA1, (uint8_t *)audio_buffer, AUDIO_FRAMES * 2U) != HAL_OK) {
    printf("I2S DMA: FAILED\r\n");
    Error_Handler();
  }
  printf("Audio: %s (48kHz)\r\n", g_wav_mode ? "WAV" : "melody");

  /* USER CODE END 3 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    if (g_sd_test_done) {
      g_sd_test_done = 0;

      if (g_wav_mode == 0U) {
        {
          FIL fil;
          FRESULT fr;
          if (f_open(&fil, "test.txt", FA_CREATE_ALWAYS | FA_WRITE) == FR_OK) {
            UINT bw = 0;
            fr = f_write(&fil, "Hello from STM32H753ZI!\r\n", 22, &bw);
            if (fr == FR_OK) {
              fr = f_sync(&fil);
            }
            f_close(&fil);
            if (fr == FR_OK) {
              printf("Wrote %u bytes to test.txt\r\n", bw);
            } else {
              printf("Write: FAILED (FR=%d, bw=%u)\r\n", (int)fr, bw);
            }
          } else {
            printf("Write: FAILED\r\n");
          }
        }

        {
          FIL fil;
          char buf[64] = {0};
          FRESULT fr;
          if (f_open(&fil, "test.txt", FA_READ) == FR_OK) {
            UINT br = 0;
            fr = f_read(&fil, buf, sizeof(buf) - 1, &br);
            f_close(&fil);
            if (fr == FR_OK) {
              printf("Read (%u): %s\r\n", br, buf);
            } else {
              printf("Read: FAILED (FR=%d)\r\n", (int)fr);
            }
          } else {
            printf("Read: FAILED\r\n");
          }
        }

        {
          DIR dir;
          FILINFO fno;
#if (AUDIO_OUTPUT_MODE == AUDIO_MODE_TEST)
          printf("Files on SD:\r\n");
#else
          printf("WAV files on SD:\r\n");
#endif
          if (f_opendir(&dir, "/") == FR_OK) {
            while (f_readdir(&dir, &fno) == FR_OK) {
              if (fno.fname[0] == 0) break;
#if (AUDIO_OUTPUT_MODE == AUDIO_MODE_TEST)
              if (!(fno.fattrib & AM_DIR)) {
                printf("  %-20s %lu\r\n", fno.fname, (uint32_t)fno.fsize);
              }
#else
              if (!(fno.fattrib & AM_DIR) && WAV_HasExt(fno.fname)) {
                printf("  %-20s %lu\r\n", fno.fname, (uint32_t)fno.fsize);
              }
#endif
            }
            f_closedir(&dir);
          }
        }

        printf("SD check done.\r\n");
      } else {
        printf("SD test skipped during WAV streaming.\r\n");
      }

      while (1) {
        HAL_Delay(1000);
      }
    }
  }
  /* USER CODE END 3 */
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
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

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
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
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
  hspi1.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
  hspi1.Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi1.Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi1.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
  hspi1.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
  hspi1.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
  hspi1.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_DISABLE;
  hspi1.Init.IOSwap = SPI_IO_SWAP_DISABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();

  /* Keep SD CS deselected: SD_CS(PE14/D4) */
  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_14, GPIO_PIN_SET);

  /* Configure PE14 (SD_CS, Arduino D4) as output */
  GPIO_InitStruct.Pin = GPIO_PIN_14;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
