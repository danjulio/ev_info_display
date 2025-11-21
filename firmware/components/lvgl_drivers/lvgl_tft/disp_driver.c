/**
 * @file disp_driver.c
 */
#include "disp_driver.h"
#include "ST7701S.h"
#include "mem_fb.h"


static bool enable_dump;



void disp_driver_init(lv_disp_drv_t* disp_drv)
{
	LCD_Init(disp_drv);
	mem_fb_init();
	enable_dump = false;
}


void disp_driver_flush(lv_disp_drv_t * drv, const lv_area_t * area, lv_color_t * color_map)
{
	if (enable_dump) {
		mem_fb_flush(drv, area, color_map);
	} else {
		lvgl_flush_cb(drv, area, color_map);
	}
}


void disp_driver_en_dump(bool en_dump)
{
	enable_dump = en_dump;
}


void disp_driver_set_bl(uint8_t brightness)
{
	Set_Backlight(brightness);
}


uint8_t disp_driver_get_bl()
{
	return Get_Backlight();
}
