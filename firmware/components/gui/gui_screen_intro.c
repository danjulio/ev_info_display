/*
 * Start-up screen displayed when the system first boots.
 *
 * Copyright 2025 Dan Julio
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * It is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software.  If not, see <https://www.gnu.org/licenses/>.
 *
 */
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gui_screen_intro.h"
#include "gui_task.h"



//
// Local variables
//

// State

// LVGL Objects
static lv_obj_t* page;
static lv_obj_t* img;

static lv_timer_t* timer = NULL;


// External image definition
LV_IMG_DECLARE(gui_intro_screen);



//
// Forward declarations for internal functions
//
static void _gui_screen_into_timer_cb(lv_timer_t * timer);



//
// API
//
lv_obj_t* gui_screen_intro_init()
{
	uint16_t w;
	uint16_t h;
	
	gui_get_screen_size(&w, &h);
	
	// Create the top-level page object
	page = lv_obj_create(NULL);
	lv_obj_set_pos(page, 0, 0);
	lv_obj_set_size(page, w, h);
	
	// Create the image that comprises the intro screen
	img = lv_img_create(page);
	lv_obj_set_size(img, w, h);
	lv_obj_set_pos(img, 0, 0);
	lv_img_set_src(img, &gui_intro_screen);
	
	return page;
}


void gui_screen_intro_set_active(bool is_active)
{
	if (is_active) {
//		lv_obj_clear_flag(page, LV_OBJ_FLAG_HIDDEN);
	
		// Start timer
		if (timer == NULL) {
			timer = lv_timer_create(_gui_screen_into_timer_cb, GUI_SCREEN_INTRO_TO_MSEC, NULL);
			lv_timer_set_repeat_count(timer, 1);  // Single-shot timer
		} else {
			// Restart existing timer
			lv_timer_set_period(timer, GUI_SCREEN_INTRO_TO_MSEC);
		}
	}
}


//
// Internal functions
//
static void _gui_screen_into_timer_cb(lv_timer_t * timer)
{
	// Delete the timer reference
	if (timer != NULL) {
		timer = NULL;
	}
	
	// Let gui_task know our display timer expired and we can be replaced
	xTaskNotify(task_handle_gui, GUI_NOTIFY_INTRO_DONE, eSetBits);
}
