#include "touch_4wire.h"

static ADC_HandleTypeDef *touch_hadc;

#define TOUCH_MEDIAN_SAMPLES 5U
#define TOUCH_RAW_VALID_MIN  50U
#define TOUCH_RAW_VALID_MAX 4000U
#define TOUCH_SETTLE_MS       1U

static bool Touch_IsRawValid(uint16_t x, uint16_t y)
{
    return (x > TOUCH_RAW_VALID_MIN && x < TOUCH_RAW_VALID_MAX &&
            y > TOUCH_RAW_VALID_MIN && y < TOUCH_RAW_VALID_MAX);
}

/* Internal GPIO/ADC Helper Macros */
#define TOUCH_YD_HI()           HAL_GPIO_WritePin(TOUCH_YD_GPIO_Port, TOUCH_YD_Pin, GPIO_PIN_SET)
#define TOUCH_YD_LO()           HAL_GPIO_WritePin(TOUCH_YD_GPIO_Port, TOUCH_YD_Pin, GPIO_PIN_RESET)
#define TOUCH_YU_HI()           HAL_GPIO_WritePin(TOUCH_YU_GPIO_Port, TOUCH_YU_Pin, GPIO_PIN_SET)
#define TOUCH_YU_LO()           HAL_GPIO_WritePin(TOUCH_YU_GPIO_Port, TOUCH_YU_Pin, GPIO_PIN_RESET)
#define TOUCH_XL_HI()           HAL_GPIO_WritePin(TOUCH_XL_GPIO_Port, TOUCH_XL_Pin, GPIO_PIN_SET)
#define TOUCH_XL_LO()           HAL_GPIO_WritePin(TOUCH_XL_GPIO_Port, TOUCH_XL_Pin, GPIO_PIN_RESET)
#define TOUCH_XR_HI()           HAL_GPIO_WritePin(TOUCH_XR_GPIO_Port, TOUCH_XR_Pin, GPIO_PIN_SET)
#define TOUCH_XR_LO()           HAL_GPIO_WritePin(TOUCH_XR_GPIO_Port, TOUCH_XR_Pin, GPIO_PIN_RESET)

static void Touch_ConfigGPIO(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, uint32_t Mode)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_Pin;
    GPIO_InitStruct.Mode = Mode;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
}

static uint16_t ADC_ReadChannel(uint32_t channel)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = channel;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_810CYCLES_5;
    sConfig.SingleDiff = ADC_SINGLE_ENDED;
    sConfig.OffsetNumber = ADC_OFFSET_NONE;
    if (HAL_ADC_ConfigChannel(touch_hadc, &sConfig) != HAL_OK) return 0;

    HAL_ADC_Start(touch_hadc);
    if (HAL_ADC_PollForConversion(touch_hadc, 10) != HAL_OK) {
        HAL_ADC_Stop(touch_hadc);
        return 0;
    }
    uint16_t val = (uint16_t)HAL_ADC_GetValue(touch_hadc);
    HAL_ADC_Stop(touch_hadc);
    return val;
}

void Touch4W_Init(ADC_HandleTypeDef *hadc)
{
    touch_hadc = hadc;
    HAL_ADCEx_Calibration_Start(touch_hadc, ADC_CALIB_OFFSET_LINEARITY, ADC_SINGLE_ENDED);
    /* Set all pins to analog (high impedance) initially */
    Touch_ConfigGPIO(TOUCH_YD_GPIO_Port, TOUCH_YD_Pin, GPIO_MODE_ANALOG);
    Touch_ConfigGPIO(TOUCH_XL_GPIO_Port, TOUCH_XL_Pin, GPIO_MODE_ANALOG);
    Touch_ConfigGPIO(TOUCH_YU_GPIO_Port, TOUCH_YU_Pin, GPIO_MODE_ANALOG);
    Touch_ConfigGPIO(TOUCH_XR_GPIO_Port, TOUCH_XR_Pin, GPIO_MODE_ANALOG);
}

static void sort_array(uint16_t *array, uint8_t size)
{
    for (uint8_t i = 0; i < size - 1; i++) {
        for (uint8_t j = 0; j < size - i - 1; j++) {
            if (array[j] > array[j + 1]) {
                uint16_t temp = array[j];
                array[j] = array[j + 1];
                array[j + 1] = temp;
            }
        }
    }
}

static uint16_t ADC_ReadMedian(uint32_t channel)
{
    uint16_t samples[TOUCH_MEDIAN_SAMPLES];
    for (uint8_t i = 0; i < TOUCH_MEDIAN_SAMPLES; i++) {
        samples[i] = ADC_ReadChannel(channel);
    }
    sort_array(samples, TOUCH_MEDIAN_SAMPLES);
    return samples[TOUCH_MEDIAN_SAMPLES / 2];
}

bool Touch4W_IsPressed(void)
{
    /* Press detect using digital path (non-floating):
       Drive XL low, read YU with pull-up input.
       Not pressed => YU stays HIGH. Pressed => YU pulled LOW through panel.
    */
    Touch_ConfigGPIO(TOUCH_YD_GPIO_Port, TOUCH_YD_Pin, GPIO_MODE_ANALOG);
    Touch_ConfigGPIO(TOUCH_XR_GPIO_Port, TOUCH_XR_Pin, GPIO_MODE_ANALOG);

    Touch_ConfigGPIO(TOUCH_XL_GPIO_Port, TOUCH_XL_Pin, GPIO_MODE_OUTPUT_PP);
    TOUCH_XL_LO();

    Touch_ConfigGPIO(TOUCH_YU_GPIO_Port, TOUCH_YU_Pin, GPIO_MODE_INPUT);
    HAL_Delay(1);

    GPIO_PinState yu_state = HAL_GPIO_ReadPin(TOUCH_YU_GPIO_Port, TOUCH_YU_Pin);

    Touch_ConfigGPIO(TOUCH_YU_GPIO_Port, TOUCH_YU_Pin, GPIO_MODE_ANALOG);
    Touch_ConfigGPIO(TOUCH_XL_GPIO_Port, TOUCH_XL_Pin, GPIO_MODE_ANALOG);

    return (yu_state == GPIO_PIN_RESET);
}

void Touch4W_GetRaw(touch_raw_t *raw)
{
    Touch4W_ReadRaw(raw);
}

bool Touch4W_GetCalibratedXY(uint16_t *x, uint16_t *y, const touch_calibration_t *cal, uint16_t width, uint16_t height)
{
    touch_raw_t raw;
    if (!Touch4W_ReadRaw(&raw)) return false;

    uint32_t tx, ty;
    uint16_t raw_for_x = cal->swap_xy ? raw.y : raw.x;
    uint16_t raw_for_y = cal->swap_xy ? raw.x : raw.y;

    if (raw_for_x <= cal->x_min) tx = 0;
    else if (raw_for_x >= cal->x_max) tx = width - 1;
    else tx = (uint32_t)((uint64_t)(raw_for_x - cal->x_min) * (width - 1) / (cal->x_max - cal->x_min));

    if (raw_for_y <= cal->y_min) ty = 0;
    else if (raw_for_y >= cal->y_max) ty = height - 1;
    else ty = (uint32_t)((uint64_t)(raw_for_y - cal->y_min) * (height - 1) / (cal->y_max - cal->y_min));

    if (cal->invert_x) tx = width - 1 - tx;
    if (cal->invert_y) ty = height - 1 - ty;

    *x = (uint16_t)tx;
    *y = (uint16_t)ty;
    return true;
}

bool Touch4W_ReadRaw(touch_raw_t *raw)
{
    /* Always check if pressed before reading ADC to avoid ghosting */
    if (!Touch4W_IsPressed()) return false;

    /* PHASE 1: X-COORDINATE
       Drive XL=HI, XR=LO. Read YU or YD via ADC.
    */
    Touch_ConfigGPIO(TOUCH_YD_GPIO_Port, TOUCH_YD_Pin, GPIO_MODE_ANALOG);
    Touch_ConfigGPIO(TOUCH_YU_GPIO_Port, TOUCH_YU_Pin, GPIO_MODE_ANALOG);
    Touch_ConfigGPIO(TOUCH_XL_GPIO_Port, TOUCH_XL_Pin, GPIO_MODE_OUTPUT_PP);
    Touch_ConfigGPIO(TOUCH_XR_GPIO_Port, TOUCH_XR_Pin, GPIO_MODE_OUTPUT_PP);

    TOUCH_XL_HI();
    TOUCH_XR_LO();
    HAL_Delay(TOUCH_SETTLE_MS);

    uint16_t x_raw = ADC_ReadMedian(TOUCH_ADC_YU_CHANNEL);

    /* PHASE 2: Y-COORDINATE
       Drive YU=HI, YD=LO. Read XL or XR via ADC.
    */
    /* Reset to Analog first to avoid ghosting */
    Touch_ConfigGPIO(TOUCH_XL_GPIO_Port, TOUCH_XL_Pin, GPIO_MODE_ANALOG);
    Touch_ConfigGPIO(TOUCH_XR_GPIO_Port, TOUCH_XR_Pin, GPIO_MODE_ANALOG);

    Touch_ConfigGPIO(TOUCH_YD_GPIO_Port, TOUCH_YD_Pin, GPIO_MODE_OUTPUT_PP);
    Touch_ConfigGPIO(TOUCH_YU_GPIO_Port, TOUCH_YU_Pin, GPIO_MODE_OUTPUT_PP);

    TOUCH_YD_LO();
    TOUCH_YU_HI();
    HAL_Delay(TOUCH_SETTLE_MS);

    uint16_t y_raw = ADC_ReadMedian(TOUCH_ADC_XL_CHANNEL);

    /* PHASE 3: CLEANUP */
    Touch_ConfigGPIO(TOUCH_YD_GPIO_Port, TOUCH_YD_Pin, GPIO_MODE_ANALOG);
    Touch_ConfigGPIO(TOUCH_XL_GPIO_Port, TOUCH_XL_Pin, GPIO_MODE_ANALOG);
    Touch_ConfigGPIO(TOUCH_YU_GPIO_Port, TOUCH_YU_Pin, GPIO_MODE_ANALOG);
    Touch_ConfigGPIO(TOUCH_XR_GPIO_Port, TOUCH_XR_Pin, GPIO_MODE_ANALOG);

    /* Double check press status to ensure panel was still held during ADC samples */
    if (Touch4W_IsPressed()) {
        raw->x = x_raw;
        raw->y = y_raw;
        raw->z = 1000;
        return true;
    }

    return false;
}

