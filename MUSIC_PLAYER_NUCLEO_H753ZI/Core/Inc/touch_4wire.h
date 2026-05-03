#ifndef __TOUCH_4WIRE_H
#define __TOUCH_4WIRE_H

#include "stm32h7xx_hal.h"
#include <stdbool.h>

/* Touch Pin Definitions (User Specified) */
#define TOUCH_YD_Pin       GPIO_PIN_3
#define TOUCH_YD_GPIO_Port  GPIOA
#define TOUCH_XL_Pin       GPIO_PIN_0
#define TOUCH_XL_GPIO_Port  GPIOC
#define TOUCH_YU_Pin       GPIO_PIN_3
#define TOUCH_YU_GPIO_Port  GPIOC
#define TOUCH_XR_Pin       GPIO_PIN_1
#define TOUCH_XR_GPIO_Port  GPIOB

/* ADC Channels for Touch Pins */
#define TOUCH_ADC_YD_CHANNEL    ADC_CHANNEL_15
#define TOUCH_ADC_XL_CHANNEL    ADC_CHANNEL_10
#define TOUCH_ADC_YU_CHANNEL    ADC_CHANNEL_13
#define TOUCH_ADC_XR_CHANNEL    ADC_CHANNEL_5

typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t z;
} touch_raw_t;

typedef struct {
    uint16_t x_min;
    uint16_t x_max;
    uint16_t y_min;
    uint16_t y_max;
    bool invert_x;
    bool invert_y;
    bool swap_xy;
} touch_calibration_t;

void Touch4W_Init(ADC_HandleTypeDef *hadc);
bool Touch4W_IsPressed(void);
void Touch4W_GetRaw(touch_raw_t *raw);
bool Touch4W_GetCalibratedXY(uint16_t *x, uint16_t *y, const touch_calibration_t *cal, uint16_t width, uint16_t height);
bool Touch4W_ReadRaw(touch_raw_t *raw);

#endif /* __TOUCH_4WIRE_H */
