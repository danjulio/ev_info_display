/**
 * @file disp_driver.h
 */

#ifndef DISP_DRIVER_H
#define DISP_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include <stdbool.h>
#include "lvgl.h"


/*********************
 *      DEFINES
 *********************/
 

/**********************
 *      TYPEDEFS
 **********************/
 

/**********************
 * GLOBAL PROTOTYPES
 **********************/
void disp_driver_init(lv_disp_drv_t* disp_drv);
void disp_driver_flush(lv_disp_drv_t * drv, const lv_area_t * area, lv_color_t * color_map);
void disp_driver_en_dump(bool en_dump);
void disp_driver_set_bl(uint8_t brightness);
uint8_t disp_driver_get_bl();


/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*DISP_DRIVER_H*/
