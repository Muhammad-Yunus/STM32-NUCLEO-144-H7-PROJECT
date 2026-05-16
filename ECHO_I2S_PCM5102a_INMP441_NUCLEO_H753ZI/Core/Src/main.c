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
#include "SEGGER_RTT.h"
#include <string.h>
#include <math.h>
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

SAI_HandleTypeDef hsai_BlockA1;
SAI_HandleTypeDef hsai_BlockB1;
DMA_HandleTypeDef hdma_sai1_a;
DMA_HandleTypeDef hdma_sai2_a;
ADC_HandleTypeDef hadc1;

/* USER CODE BEGIN PV */
/* 32-bit stereo buffers in DMA-accessible RAM */
int32_t mic_buffer[MIC_BUFFER_SIZE * 2]    __attribute__((aligned(32)));
int32_t play_buffer[MIC_BUFFER_SIZE * 2]   __attribute__((aligned(32)));
volatile uint32_t callback_count = 0;
volatile int32_t  mic_peak    = 0;
volatile int32_t  mic_raw_val = 0;
volatile uint32_t zc_count    = 0;
int32_t zc_prev  = 0;
volatile uint32_t volume_adc  = 2048;  /* 0..4095, default mid */

/* Parrot mode: capture last 4s, play once after silence */
#define AUDIO_FS_HZ 16000
#define PARROT_SAMPLES (AUDIO_FS_HZ * 6)
int32_t parrot_buffer[PARROT_SAMPLES] __attribute__((aligned(32)));
volatile uint32_t parrot_write_idx = 0;
volatile uint8_t  parrot_filled = 0;
volatile uint32_t parrot_play_idx = 0;
volatile uint32_t parrot_play_remaining = 0;
volatile uint8_t  parrot_mode_play = 0; /* 0=record/listen, 1=playback */
volatile uint32_t silence_samples = 0;
static float parrot_env = 0.0f; /* smoothed envelope for trigger */
volatile uint8_t  parrot_button_record = 0;
volatile uint8_t  btn_prev = 1; /* PC13 idle high on Nucleo */
#define PARROT_TRIGGER_TH     0.005f /* -46dBFS: trigger threshold */
#define SILENCE_HOLD_SAMPLES  (AUDIO_FS_HZ / 2) /* 0.5s */
#define PARROT_VOL_NUM          7   /* playback gain 0.7x */
#define PARROT_VOL_DEN         10

#define GOERTZEL_N 256
static float goertzel_buf[GOERTZEL_N];
static uint32_t goertzel_idx = 0;
static volatile float g300_db = -120.0f;
static volatile float g600_db = -120.0f;  /* harmonic check */
static volatile float tone_score_db = -120.0f;  /* g300 - g600 */
static volatile uint32_t tone_est_hz = 0;  /* 300 or 600 dominance */
static volatile uint32_t analysis_blocks = 0;  /* how many 256-sample blocks analysed */
static volatile float analysis_power_sum = 0.0f;
static volatile uint32_t analysis_power_count = 0;  /* for noise gate insight */

static float goertzel_mag_db(const float *x, uint32_t n, float fs, float f0)
{
    float w = 2.0f * 3.14159265359f * f0 / fs;
    float c = 2.0f * cosf(w);
    float s0 = 0.0f, s1 = 0.0f, s2 = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        s0 = x[i] + c * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    float power = s1*s1 + s2*s2 - c*s1*s2;
    if (power < 1e-20f) power = 1e-20f;
    return 10.0f * log10f(power);
}

/* Simplified De-hiss DSP: HPF (DC/Rumble) + LPF (High noise) + Noise Floor Suppressor */
typedef struct { float b0,b1,b2,a1,a2,w1,w2; } Biquad;
static Biquad hpf = { .b0= 0.9201f, .b1=-1.8402f, .b2= 0.9201f, .a1=-1.8337f, .a2= 0.8467f, .w1=0.f, .w2=0.f };
static Biquad lpf = { .b0= 0.2066f, .b1= 0.4131f, .b2= 0.2066f, .a1=-0.3695f, .a2= 0.1958f, .w1=0.f, .w2=0.f };

/* Noise floor suppressor states */
#define HISS_HARD_TH   0.0008f  /* Lighter hard mute */
#define HISS_SOFT_TH   0.0022f  /* Lighter soft fade */

static inline float biquad_process(Biquad *f, float x) {
    float w = x - f->a1*f->w1 - f->a2*f->w2;
    float y = f->b0*w + f->b1*f->w1 + f->b2*f->w2;
    f->w2 = f->w1; f->w1 = w;
    return y;
}
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SAI1B_Init(void);
static void MX_SAI1_Init(void);
static void MX_ADC1_Init(void);
/* USER CODE BEGIN PFP */
void process_audio(uint32_t offset, uint32_t size);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* Double-buffer passthrough: mic_buffer and play_buffer both MIC_BUFFER_SIZE*2 elements.
 * TX DMA count = MIC_BUFFER_SIZE*2 (same as RX) so both loop at same rate.
 * RX half-cplt fills mic[0..255], we write play[0..255].
 * RX cplt fills mic[256..511], we write play[256..511]. No overflow. */
/* Double-buffer passthrough: mic_buffer and play_buffer both MIC_BUFFER_SIZE*2 elements.
 * offset and size provided by HAL are in samples.
 * SAI DMA is configured for 32-bit (word) transfers. */
void process_audio(uint32_t offset, uint32_t size) {
    int32_t local_peak = 0;
    /* Clamp volume to max 60% */
    uint32_t vol_clamped = (volume_adc > 2457) ? 2457 : volume_adc;
    float vol_gain = (float)vol_clamped / 4096.0f;

    /* size is number of elements in current half (512 words)
     * For 32-bit I2S: L is at offset+0, R is at offset+1, offset+2... */
    for (uint32_t i = 0; i < size; i += 2) {
        uint32_t idx = offset + i;
        /* INMP441: 24-bit in bits[31:8] of left slot */
        int32_t raw = mic_buffer[idx] >> 8;
        int32_t mic_sample;

#if ENABLE_AUDIO_PARROT
        if (!parrot_mode_play) {
            float sample_f = (float)raw / 8388608.0f;
#if ENABLE_AUDIO_DSP
            /* Denoise chain for Parrot record: HPF + LPF + gate */
            float den = biquad_process(&hpf, sample_f);
            den = biquad_process(&lpf, den);
            float den_abs = (den < 0.0f) ? -den : den;
            if (den_abs < 0.0015f) den = 0.0f;          /* hard mute tiny hiss */
            else if (den_abs < 0.0030f) den *= 0.35f;   /* soft attenuation near floor */
            sample_f = den;
#endif
            float abs_f = (sample_f < 0.0f) ? -sample_f : sample_f;
            parrot_env += 0.01f * (abs_f - parrot_env);
            analysis_power_sum += sample_f * sample_f;
            analysis_power_count++;

            if (parrot_button_record) {
                /* RECORD exactly 6 seconds after button press */
                int32_t rec_s = (int32_t)(sample_f * 8388607.0f);
                parrot_buffer[parrot_write_idx++] = rec_s;
                if (parrot_write_idx >= PARROT_SAMPLES) {
                    parrot_filled = 1;
                    parrot_button_record = 0;
                    parrot_mode_play = 1;
                    parrot_play_idx = 0;
                    parrot_play_remaining = PARROT_SAMPLES;
                }
            }
            mic_sample = 0; /* mute live during listening/record */
        } else {
            /* PLAY once */
            int32_t p = parrot_buffer[parrot_play_idx++];
            mic_sample = (int32_t)((float)p * vol_gain * (float)PARROT_VOL_NUM / (float)PARROT_VOL_DEN);
            if (parrot_play_idx >= PARROT_SAMPLES) parrot_play_idx = 0;

            if (--parrot_play_remaining == 0) {
                parrot_mode_play = 0;
                parrot_write_idx = 0;
                parrot_filled = 0;
                parrot_env = 0;
            }
        }
#else
#if ENABLE_AUDIO_DSP
        float sample_f = (float)raw / 8388608.0f;
        /* De-hiss only */
        float filtered = biquad_process(&hpf, sample_f);
        filtered = biquad_process(&lpf, filtered);

        float abs_f = (filtered < 0.0f) ? -filtered : filtered;
        if (abs_f < HISS_HARD_TH) {
            filtered = 0.0f;
        } else if (abs_f < HISS_SOFT_TH) {
            float gain = (abs_f - HISS_HARD_TH) / (HISS_SOFT_TH - HISS_HARD_TH);
            filtered *= gain;
        }

        filtered *= vol_gain;
        if (filtered > 0.79f) filtered = 0.79f;
        if (filtered < -0.79f) filtered = -0.79f;
        mic_sample = (int32_t)(filtered * 8388607.0f);
#else
        mic_sample = (int32_t)((float)raw * vol_gain);
#endif
#endif


        /* Output to DAC */
        int32_t final_out = mic_sample << 8;
        play_buffer[idx]     = final_out;
        play_buffer[idx + 1] = final_out;

        if (abs(mic_sample) > local_peak) local_peak = abs(mic_sample);
    }
    if (local_peak > mic_peak) mic_peak = local_peak;
    mic_raw_val = mic_buffer[offset];
}

void HAL_SAI_RxHalfCpltCallback(SAI_HandleTypeDef *hsai) {
    if (hsai->Instance == SAI1_Block_B)
        process_audio(0, MIC_BUFFER_SIZE);
}
void HAL_SAI_RxCpltCallback(SAI_HandleTypeDef *hsai) {
    if (hsai->Instance == SAI1_Block_B) {
        process_audio(MIC_BUFFER_SIZE, MIC_BUFFER_SIZE);
        callback_count++;
    }
}
void HAL_SAI_TxHalfCpltCallback(SAI_HandleTypeDef *hsai) {}
void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai) {}
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
  SCB_EnableICache();
  /* KEEP D-CACHE DISABLED to avoid audio crackling/coherency issues */
//  SCB_DisableDCache();

  /* Enable the SYSCFG clock */
  __HAL_RCC_SYSCFG_CLK_ENABLE();
  /* Connect PC2 and PC3 to the ADC/Digital via the analog switch */
  HAL_SYSCFG_AnalogSwitchConfig(SYSCFG_SWITCH_PC2, SYSCFG_SWITCH_PC2_OPEN);
  HAL_SYSCFG_AnalogSwitchConfig(SYSCFG_SWITCH_PC3, SYSCFG_SWITCH_PC3_OPEN);
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  SEGGER_RTT_Init();
  SEGGER_RTT_printf(0, "Booting...\n");

  MX_SAI1_Init();
  MX_SAI1B_Init();
  /* USER CODE BEGIN 2 */
  SEGGER_RTT_Init();
  SEGGER_RTT_printf(0, "\n\n=== AUDIO TEST START ===\n");

  memset(mic_buffer, 0, sizeof(mic_buffer));
  memset(play_buffer, 0, sizeof(play_buffer));
//  SCB_CleanDCache_by_Addr((uint32_t*)play_buffer, sizeof(play_buffer));

  /* Start RX (SAI1_Block_B - slave, synchronous with Block_A) */
  if (HAL_SAI_Receive_DMA(&hsai_BlockB1, (uint8_t*)mic_buffer, MIC_BUFFER_SIZE * 2) != HAL_OK) {
      SEGGER_RTT_printf(0, "!! SAI1B DMA FAIL !!\n");
      Error_Handler();
  }
  SEGGER_RTT_printf(0, "SAI1B Recording Started (sync slave)\n");

  /* Start TX (SAI1 - master, generates clock for PCM5102a) */
  if (HAL_SAI_Transmit_DMA(&hsai_BlockA1, (uint8_t*)play_buffer, MIC_BUFFER_SIZE * 2) != HAL_OK) {
      SEGGER_RTT_printf(0, "!! SAI1 DMA FAIL !!\n");
      Error_Handler();
  }
  SEGGER_RTT_printf(0, "SAI1 Streaming Started (master)\n");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint32_t last_tick = HAL_GetTick();
  uint32_t last_adc_tick = 0;
  while (1)
  {
    /* Read user button PC13 every 20ms */
    if (HAL_GetTick() - last_adc_tick >= 20) {
        /* Potentiometer */
        HAL_ADC_Start(&hadc1);
        if (HAL_ADC_PollForConversion(&hadc1, 2) == HAL_OK)
            volume_adc = HAL_ADC_GetValue(&hadc1);
        HAL_ADC_Stop(&hadc1);

        /* Button PC13: Trigger Record 4s */
        uint8_t btn = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13);
        /* Nucleo user button is usually Active High (B1 USER) */
        if (btn == GPIO_PIN_SET && btn_prev == GPIO_PIN_RESET) {
            if (!parrot_mode_play && !parrot_button_record) {
                parrot_button_record = 1;
                parrot_write_idx = 0;
                parrot_filled = 0;
            }
        }
        btn_prev = btn;

        last_adc_tick = HAL_GetTick();
    }
    if (HAL_GetTick() - last_tick > 1000) {
        float avg_power = (analysis_power_count > 0) ? (analysis_power_sum / analysis_power_count) : 0;
        int32_t rms_db10 = (int32_t)(10.0f * log10f(avg_power + 1e-12f) * 10.0f);

        SEGGER_RTT_printf(0, "D:%u|VOL:%u|RMS:%ld.%01lddB|Mode:%s|Env:%lu|Idx:%u/%u\n",
            callback_count, volume_adc,
            (long)(rms_db10/10), (long)labs(rms_db10%10),
            parrot_mode_play ? "PLAY" : (parrot_button_record ? "RECORD" : "IDLE"),
            (unsigned long)(parrot_env * 1000000.0f),
            parrot_mode_play ? (unsigned int)parrot_play_remaining : (unsigned int)parrot_write_idx,
            PARROT_SAMPLES);

        callback_count = 0;
        mic_peak = 0;
        zc_count = 0;
        analysis_power_sum = 0;
        analysis_power_count = 0;
        last_tick = HAL_GetTick();
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

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

  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /* Configure PLL2 for Audio (12.288 MHz kernel clock) from HSE 8MHz */
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SAI1|RCC_PERIPHCLK_SPI2;
  PeriphClkInitStruct.Sai1ClockSelection = RCC_SAI1CLKSOURCE_PLL2;
  PeriphClkInitStruct.Spi123ClockSelection = RCC_SPI123CLKSOURCE_PLL2;
  PeriphClkInitStruct.PLL2.PLL2M = 1;
  PeriphClkInitStruct.PLL2.PLL2N = 24;
  PeriphClkInitStruct.PLL2.PLL2P = 16;
  PeriphClkInitStruct.PLL2.PLL2Q = 2;
  PeriphClkInitStruct.PLL2.PLL2R = 2;
  PeriphClkInitStruct.PLL2.PLL2RGE = RCC_PLL2VCIRANGE_3;
  PeriphClkInitStruct.PLL2.PLL2VCOSEL = RCC_PLL2VCOWIDE;
  PeriphClkInitStruct.PLL2.PLL2FRACN = 4793;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initialises the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2|RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
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

static void MX_SAI1_Init(void)
{
  hsai_BlockA1.Instance = SAI1_Block_A;
  hsai_BlockA1.Init.AudioMode = SAI_MODEMASTER_TX;
  hsai_BlockA1.Init.Synchro = SAI_ASYNCHRONOUS;  /* Independent master */
  hsai_BlockA1.Init.OutputDrive = SAI_OUTPUTDRIVE_ENABLE;
  hsai_BlockA1.Init.NoDivider = SAI_MASTERDIVIDER_ENABLE;
  hsai_BlockA1.Init.FIFOThreshold = SAI_FIFOTHRESHOLD_HF;
  hsai_BlockA1.Init.AudioFrequency = SAI_AUDIO_FREQUENCY_16K;
  hsai_BlockA1.Init.MonoStereoMode = SAI_STEREOMODE;
  hsai_BlockA1.Init.CompandingMode = SAI_NOCOMPANDING;
  hsai_BlockA1.Init.TriState = SAI_OUTPUT_NOTRELEASED;
  hsai_BlockA1.Init.Protocol = SAI_FREE_PROTOCOL;
  hsai_BlockA1.Init.DataSize = SAI_DATASIZE_24;
  hsai_BlockA1.Init.FirstBit = SAI_FIRSTBIT_MSB;
  hsai_BlockA1.Init.ClockStrobing = SAI_CLOCKSTROBING_FALLINGEDGE;

  /* Frame Config - 64 bits (32 per channel) */
  hsai_BlockA1.FrameInit.FrameLength = 64;
  hsai_BlockA1.FrameInit.ActiveFrameLength = 32;
  hsai_BlockA1.FrameInit.FSDefinition = SAI_FS_CHANNEL_IDENTIFICATION;
  hsai_BlockA1.FrameInit.FSPolarity = SAI_FS_ACTIVE_LOW;
  hsai_BlockA1.FrameInit.FSOffset = SAI_FS_BEFOREFIRSTBIT;

  /* Slot Config */
  hsai_BlockA1.SlotInit.FirstBitOffset = 0;
  hsai_BlockA1.SlotInit.SlotSize = SAI_SLOTSIZE_32B;
  hsai_BlockA1.SlotInit.SlotNumber = 2;
  hsai_BlockA1.SlotInit.SlotActive = SAI_SLOTACTIVE_0 | SAI_SLOTACTIVE_1;

  if (HAL_SAI_Init(&hsai_BlockA1) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_SAI1B_Init(void)
{
  hsai_BlockB1.Instance = SAI1_Block_B;
  hsai_BlockB1.Init.AudioMode = SAI_MODESLAVE_RX;
  hsai_BlockB1.Init.Synchro = SAI_SYNCHRONOUS;
  hsai_BlockB1.Init.OutputDrive = SAI_OUTPUTDRIVE_DISABLE;
  hsai_BlockB1.Init.NoDivider = SAI_MASTERDIVIDER_DISABLE;
  hsai_BlockB1.Init.FIFOThreshold = SAI_FIFOTHRESHOLD_HF;
  hsai_BlockB1.Init.AudioFrequency = SAI_AUDIO_FREQUENCY_16K;
  hsai_BlockB1.Init.MonoStereoMode = SAI_STEREOMODE;
  hsai_BlockB1.Init.CompandingMode = SAI_NOCOMPANDING;
  hsai_BlockB1.Init.TriState = SAI_OUTPUT_NOTRELEASED;
  hsai_BlockB1.Init.Protocol = SAI_FREE_PROTOCOL;
  hsai_BlockB1.Init.DataSize = SAI_DATASIZE_32;
  hsai_BlockB1.Init.FirstBit = SAI_FIRSTBIT_MSB;
  hsai_BlockB1.Init.ClockStrobing = SAI_CLOCKSTROBING_RISINGEDGE;

  /* Frame Config - same as Block A */
  hsai_BlockB1.FrameInit.FrameLength = 64;
  hsai_BlockB1.FrameInit.ActiveFrameLength = 32;
  hsai_BlockB1.FrameInit.FSDefinition = SAI_FS_CHANNEL_IDENTIFICATION;
  hsai_BlockB1.FrameInit.FSPolarity = SAI_FS_ACTIVE_LOW;
  hsai_BlockB1.FrameInit.FSOffset = SAI_FS_BEFOREFIRSTBIT;

  /* Slot Config */
  hsai_BlockB1.SlotInit.FirstBitOffset = 0;
  hsai_BlockB1.SlotInit.SlotSize = SAI_SLOTSIZE_32B;
  hsai_BlockB1.SlotInit.SlotNumber = 2;
  hsai_BlockB1.SlotInit.SlotActive = SAI_SLOTACTIVE_0 | SAI_SLOTACTIVE_1;

  if (HAL_SAI_Init(&hsai_BlockB1) != HAL_OK)
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
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /* Configure PC13 (User Button) */
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
static void MX_ADC1_Init(void)
{
    /* Enable clocks */
    __HAL_RCC_ADC12_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* PA3 → analog input (no pull, no AF) */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin  = GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* ADC1 config */
    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler        = ADC_CLOCK_ASYNC_DIV2;
    hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode          = ADC_SCAN_DISABLE;
    hadc1.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
    hadc1.Init.LowPowerAutoWait      = DISABLE;
    hadc1.Init.ContinuousConvMode    = DISABLE;
    hadc1.Init.NbrOfConversion       = 1;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DR;
    hadc1.Init.Overrun               = ADC_OVR_DATA_OVERWRITTEN;
    hadc1.Init.LeftBitShift          = ADC_LEFTBITSHIFT_NONE;
    hadc1.Init.OversamplingMode      = DISABLE;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) Error_Handler();

    /* Run calibration */
    if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED) != HAL_OK)
        Error_Handler();

    /* Channel 15 = PA3 */
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel      = ADC_CHANNEL_15;
    sConfig.Rank         = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_64CYCLES_5;
    sConfig.SingleDiff   = ADC_SINGLE_ENDED;
    sConfig.OffsetNumber = ADC_OFFSET_NONE;
    sConfig.Offset       = 0;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) Error_Handler();
}
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
