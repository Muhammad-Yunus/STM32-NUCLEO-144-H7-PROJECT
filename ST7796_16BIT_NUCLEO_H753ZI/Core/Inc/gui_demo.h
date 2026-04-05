/**
 ******************************************************************************
 * @file    gui_demo.h
 * @brief   Demo functions showcasing the GUI capabilities.
 ******************************************************************************
 */

#ifndef __GUI_DEMO_H
#define __GUI_DEMO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Simple color loop – already in gui_demo.c */
void simple_color_loop_test(void);

/* Touch calibration demo */
void touch_calib_demo(void);

/* Full demo suite – calls all individual demos */
void gui_demo_all(void);

#ifdef __cplusplus
}
#endif

#endif /* __GUI_DEMO_H */
