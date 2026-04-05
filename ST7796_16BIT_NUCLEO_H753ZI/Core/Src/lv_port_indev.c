/**
 * @file lv_port_indev.c
 * LVGL touch input device driver for XPT2046 controller
 */

#include "lv_port_indev.h"
#include "touch.h"

static void touchpad_read_cb(lv_indev_t *indev_drv, lv_indev_data_t *data)
{
    /* Scan touch coordinates */
    uint8_t touch = TP_Scan(0);    /* screen mode */

    if (touch & TP_PRES_DOWN)
    {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = tp_dev.x;
        data->point.y = tp_dev.y;
    }
    else
    {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void lv_port_indev_init(void)
{
    lv_indev_t *indev;

    /* Initialize LVGL touchpad input device */
    indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touchpad_read_cb);
}
