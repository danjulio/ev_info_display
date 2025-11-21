/*
 * Main screen.
 *
 * Contains a LVGL Tile View object that displays the various information screens.
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
#include "gui_screen_main.h"
#include "gui_task.h"
#include "gui_tile_electrical.h"
#include "gui_tile_power.h"
#include "gui_tile_settings.h"
#include "gui_tile_timed.h"
#include "gui_tile_torque.h"



//
// Local constants
//



//
// Local variables
//

// State
static int num_tiles = 0;
static int cur_tile_index = 0;

// LVGL Objects
static lv_obj_t* page;
static lv_obj_t* tileview;

// Array of tile objects
static lv_obj_t* tile_list[GUI_SCREEN_MAIN_NUM_TILES];

// Array of tile object enable functions
static tile_activation_handler tile_activation_fcn_list[GUI_SCREEN_MAIN_NUM_TILES];



//
// Forward declarations for internal functions
//
static void _gui_screen_main_tileview_changed_cb(lv_event_t * event);



//
// API
//
lv_obj_t* gui_screen_main_init()
{
	uint16_t w, h;
	
	gui_get_screen_size(&w, &h);
	
	// Create the top-level page object
	page = lv_obj_create(NULL);
	lv_obj_set_pos(page, 0, 0);
	lv_obj_set_size(page, w, h);
	
	// Create the tileview object to hold the various tile pages
	tileview = lv_tileview_create(page);
	lv_obj_set_pos(tileview, 0, 0);
	lv_obj_set_size(tileview, w, h);
	lv_obj_set_scrollbar_mode(tileview, LV_SCROLLBAR_MODE_OFF);
	lv_obj_add_event_cb(tileview, _gui_screen_main_tileview_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
	
	// Initialize the tile object arrays
	for (int i=0; i<GUI_SCREEN_MAIN_NUM_TILES; i++) {
		tile_list[i] = NULL;
		tile_activation_fcn_list[i] = NULL;
	}
	
	// Add tiles to the tileview object.
	// They will register themselves with us if they can be displayed based on the vehicle capabilities.
	gui_tile_torque_init(tileview, &cur_tile_index);
	gui_tile_power_init(tileview, &cur_tile_index);
	gui_tile_electrical_init(tileview, &cur_tile_index);
	gui_tile_timed_init(tileview, &cur_tile_index);
	gui_tile_settings_init(tileview, &cur_tile_index);
	
	// Set displayed tile
	cur_tile_index = gui_get_init_tile_index();
	if ((cur_tile_index == -1) && (num_tiles > 0)) {
		// First time or new vehicle so select the first available tile and store it
		cur_tile_index = 0;
		gui_set_init_tile_index(cur_tile_index);
	}
	
	if (cur_tile_index >= 0) {
		lv_obj_set_tile_id(tileview, (uint32_t) cur_tile_index, 0, LV_ANIM_OFF);
	}
	
	return page;
}


void gui_screen_main_set_active(bool is_active)
{
	// Inform currently selected tile so it can manage state
	tile_activation_fcn_list[cur_tile_index](is_active);
}


void gui_screen_main_register_tile(lv_obj_t* tile, tile_activation_handler activate_func)
{
	if (num_tiles < GUI_SCREEN_MAIN_NUM_TILES) {
		tile_list[num_tiles] = tile;
		tile_activation_fcn_list[num_tiles] = activate_func;
		num_tiles += 1;
	}
}



//
// Internal functions
//
static void _gui_screen_main_tileview_changed_cb(lv_event_t * event)
{
	int32_t n = -1;
	lv_obj_t* active_tile;
	
	if (lv_event_get_code(event) == LV_EVENT_VALUE_CHANGED) {
		active_tile = lv_tileview_get_tile_act(tileview);
		
		// Find the tile's index
		for (int i=0; i<num_tiles; i++) {
			if (tile_list[i] == active_tile) {
				n = i;
				break;
			}
		}
		
		if ((n >= 0) && (n != cur_tile_index)) {
			// Disable previous tile
			tile_activation_fcn_list[cur_tile_index](false);
			
			// Enable new tile
			cur_tile_index = n;
			tile_activation_fcn_list[cur_tile_index](true);
			
			// Let gui_task know to update persistent storage
			gui_set_init_tile_index(cur_tile_index);
		}
	}
}