/*
 * Main system settings tile.  Display controls and update persistent storage
 * (perform system reboot after most settings are saved).
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
#include "can_manager.h"
#include "disp_driver.h"
#include "esp_system.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gui_task.h"
#include "gui_screen_main.h"
#include "gui_tile_settings.h"
#include "ps_utilities.h"
#include "vehicle_manager.h"
#include <stdlib.h>
#include <string.h>



//
// Local Constants
//

// Connection status update timer evaluation interval
#define TIMER_EVAL_MSEC       500



//
// Local Variables
//
static const char* TAG = "gui_tile_settings";

// LVGL objects
static lv_obj_t* tile;

static lv_obj_t* connection_status_lbl;

static lv_obj_t* vehicle_dd;
static lv_obj_t* vehicle_dd_lbl;

static lv_obj_t* connection_dd;
static lv_obj_t* connection_dd_lbl;

static lv_obj_t* brightness_sl;
static lv_obj_t* brightness_sl_lbl;

static lv_obj_t* units_sw;
static lv_obj_t* units_sw_lbl;

static lv_obj_t* save_btn;
static lv_obj_t* save_btn_lbl;
static lv_obj_t* save_btn_info_lbl;

static lv_obj_t* version_info_lbl;

static lv_timer_t* connection_status_eval_timer = NULL;

// State
static bool is_connected;
static bool prev_screen_settings;   // Used to restore a selection after returning from a settings screen
static char* canbus_list;
static char* vehicle_list;
static uint16_t tile_w;
static uint16_t tile_h;
static uint16_t vertical_spacing;
static uint16_t row_y;

static main_config_t* configP;

static char cur_vehicle_name[PS_VEHICLE_NAME_MAX_LEN+1];
static char new_vehicle_name[PS_VEHICLE_NAME_MAX_LEN+1];

static uint32_t cur_if_index;
static uint32_t new_if_index;

static uint8_t cur_brightness;

static bool cur_is_metric;
static bool new_is_metric;



//
// Forward declarations for internal functions
//
static void _gui_tile_settings_set_active(bool en);
static void _gui_tile_settings_setup_connection_status();
static void _gui_tile_settings_setup_vehicle();
static void _gui_tile_settings_setup_connection();
static void _gui_tile_settings_setup_brightness();
static void _gui_tile_settings_setup_units();
static void _gui_tile_settings_setup_save();
static void _gui_tile_settings_setup_version();
static void _gui_tile_settings_dd_cb(lv_event_t* e);
static void _gui_tile_settings_sl_cb(lv_event_t* e);
static void _gui_tile_settings_sw_cb(lv_event_t* e);
static void _gui_tile_settings_btn_cb(lv_event_t* e);
static void _gui_tile_settings_connection_status_timer_cb(lv_timer_t* timer);



//
// API
//
void gui_tile_settings_init(lv_obj_t* parent_tileview, int* tile_index)
{
	// Create our object
	tile = lv_tileview_add_tile(parent_tileview, *tile_index, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
	*tile_index += 1;
	
	gui_get_screen_size(&tile_w, &tile_h);
	vertical_spacing = tile_h / 7;            // Number of lines of controls + 2 for good spacing on a circular display
	row_y = 0;
	
	// Setup controls
	_gui_tile_settings_setup_connection_status();
	_gui_tile_settings_setup_vehicle();
	_gui_tile_settings_setup_connection();
	_gui_tile_settings_setup_brightness();
	_gui_tile_settings_setup_units();
	_gui_tile_settings_setup_save();
	_gui_tile_settings_setup_version();
	
	// Register ourselves
	gui_screen_main_register_tile(tile, _gui_tile_settings_set_active);
	
	// Get a pointer to the persistent storage main configuration
	(void) ps_get_config(PS_CONFIG_TYPE_MAIN, (void**) &configP);
	
	// Create our evaluation timer
	connection_status_eval_timer = lv_timer_create(_gui_tile_settings_connection_status_timer_cb, TIMER_EVAL_MSEC, NULL);
	lv_timer_set_repeat_count(connection_status_eval_timer, -1);
	lv_timer_pause(connection_status_eval_timer);
	
	// Will come to this tile from another tile the first time
	prev_screen_settings = false;
}



//
// Internal functions
//
static void _gui_tile_settings_set_active(bool en)
{
	if (en) {
		if (!prev_screen_settings) {
			// Coming from another main screen tile
			//
			// Disable all requests
			vm_set_request_item_mask(0);
			
			// Set controls based on current configuration
			//
			// Connection status
			is_connected = can_connected();
			if (is_connected) {
				lv_label_set_text_static(connection_status_lbl, LV_SYMBOL_REFRESH);				
			} else {
				lv_label_set_text_static(connection_status_lbl, "");
			}
			lv_timer_resume(connection_status_eval_timer);
			
			// Vehicle drop-down
			strncpy(cur_vehicle_name, configP->vehicle_name, PS_VEHICLE_NAME_MAX_LEN);
			strncpy(new_vehicle_name, configP->vehicle_name, PS_VEHICLE_NAME_MAX_LEN);
			for (int i=0; i<vm_get_num_vehicles(); i++) {
				if (strcmp(vm_get_vehicle_name(i), cur_vehicle_name) == 0) {
					lv_dropdown_set_selected(vehicle_dd, (uint16_t) i);
					break;
				}
			}
			
			// Connection drop-down
			cur_if_index = configP->connection_index;
			new_if_index = cur_if_index;
			lv_dropdown_set_selected(connection_dd, (uint16_t) cur_if_index);
			
			// Brightness slider
			cur_brightness = disp_driver_get_bl();
			lv_slider_set_value(brightness_sl, (int32_t) cur_brightness, false);
			
			// Metric (/Imperial) switch
			cur_is_metric = (configP->config_flags & PS_MAIN_FLAG_METRIC) == PS_MAIN_FLAG_METRIC;
			new_is_metric = cur_is_metric;
			if (cur_is_metric) {
				lv_obj_add_state(units_sw, LV_STATE_CHECKED);
			} else {
				lv_obj_clear_state(units_sw, LV_STATE_CHECKED);
			}
		} else {
			// Leave controls as they were when we displayed a setting screen
			prev_screen_settings = false;
		}
	} else {
		lv_timer_pause(connection_status_eval_timer);
	}
}


static void _gui_tile_settings_setup_connection_status()
{
	connection_status_lbl = lv_label_create(tile);
	lv_label_set_long_mode(connection_status_lbl, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_align(connection_status_lbl, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_set_style_text_font(connection_status_lbl, &lv_font_montserrat_18, LV_PART_MAIN);
	lv_obj_set_width(connection_status_lbl, tile_w);
	lv_obj_set_pos(connection_status_lbl, 0, row_y + 20);
	
	row_y += vertical_spacing;
}


static void _gui_tile_settings_setup_vehicle()
{
	char* cp;
	int i;
	int len = 0;
	int n;
	
	// First malloc some memory for the drop-down list based on how many vehicles we currently support
	n = vm_get_num_vehicles();
	for (i=0; i<n; i++) {
		len += (strlen(vm_get_vehicle_name(i)) + 1);   // Include room for trailing "\n" or null character
	}
	vehicle_list = malloc(len);
	
	// Create the list of vehicles
	cp = vehicle_list;
	for (i=0; i<n; i++) {
		strcpy(cp, vm_get_vehicle_name(i));
		cp += strlen(cp);
		if (i == (n-1)) {
			// Final entry
			*cp = 0;
		} else {
			strcpy(cp, "\n");
		}
		cp += 1;
	}
	
	// Create the control label
	vehicle_dd_lbl = lv_label_create(tile);
	lv_label_set_long_mode(vehicle_dd_lbl, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_align(vehicle_dd_lbl, LV_TEXT_ALIGN_RIGHT, 0);
	lv_obj_set_style_text_font(vehicle_dd_lbl, &lv_font_montserrat_24, LV_PART_MAIN);
	lv_obj_set_pos(vehicle_dd_lbl, 0, row_y);
	lv_obj_set_width(vehicle_dd_lbl, tile_w/2 - 5);
	lv_label_set_text_static(vehicle_dd_lbl, "Vehicle");
	
	// Create the vehicle drop-down
	vehicle_dd = lv_dropdown_create(tile);
	lv_obj_set_width(vehicle_dd, tile_w/3);
	lv_dropdown_set_options(vehicle_dd, vehicle_list);
	lv_obj_set_pos(vehicle_dd, tile_w/2 + 5, row_y);
	lv_obj_add_event_cb(vehicle_dd, _gui_tile_settings_dd_cb, LV_EVENT_VALUE_CHANGED, NULL);
	
	row_y += vertical_spacing;
}


static void _gui_tile_settings_setup_connection()
{
	char* cp;
	int i;
	int len = 0;
	int n;
	
	// First malloc some memory for the drop-down list based on how many CAN bus interfaces we currently support
	n = can_get_num_interfaces();
	for (i=0; i<n; i++) {
		len += (strlen(can_get_interface_name(i)) + 1);   // Include room for trailing "\n" or null character
	}
	canbus_list = malloc(len);
	
	// Create the list of vehicles
	cp = canbus_list;
	for (i=0; i<n; i++) {
		strcpy(cp, can_get_interface_name(i));
		cp += strlen(cp);
		if (i == (n-1)) {
			// Final entry
			*cp = 0;
		} else {
			strcpy(cp, "\n");
		}
		cp += 1;
	}
	
	// Create the control label
	connection_dd_lbl = lv_label_create(tile);
	lv_label_set_long_mode(connection_dd_lbl, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_align(connection_dd_lbl, LV_TEXT_ALIGN_RIGHT, 0);
	lv_obj_set_style_text_font(connection_dd_lbl, &lv_font_montserrat_24, LV_PART_MAIN);
	lv_obj_set_pos(connection_dd_lbl, 0, row_y);
	lv_obj_set_width(connection_dd_lbl, tile_w/2 - 5);
	lv_label_set_text_static(connection_dd_lbl, "Interface");
	
	// Create the vehicle drop-down
	connection_dd = lv_dropdown_create(tile);
	lv_obj_set_width(connection_dd, tile_w/3);
	lv_dropdown_set_options(connection_dd, canbus_list);
	lv_obj_set_pos(connection_dd, tile_w/2 + 5, row_y);
	lv_obj_add_event_cb(connection_dd, _gui_tile_settings_dd_cb, LV_EVENT_VALUE_CHANGED, NULL);
	
	row_y += vertical_spacing;
}


static void _gui_tile_settings_setup_brightness()
{
	// Create the control label
	brightness_sl_lbl = lv_label_create(tile);
	lv_label_set_long_mode(brightness_sl_lbl, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_align(brightness_sl_lbl, LV_TEXT_ALIGN_RIGHT, 0);
	lv_obj_set_style_text_font(brightness_sl_lbl, &lv_font_montserrat_24, LV_PART_MAIN);
	lv_obj_set_pos(brightness_sl_lbl, 0, row_y);
	lv_obj_set_width(brightness_sl_lbl, tile_w/2 - 5);
	lv_label_set_text_static(brightness_sl_lbl, "Brightness");
	
	// Create the brightness slider
	brightness_sl = lv_slider_create(tile);
	lv_obj_set_width(brightness_sl, tile_w/2 - 50);
	lv_obj_set_pos(brightness_sl, tile_w/2 + 5, row_y + 10);
	lv_slider_set_range(brightness_sl, 10, 100);
	lv_obj_add_event_cb(brightness_sl, _gui_tile_settings_sl_cb, LV_EVENT_VALUE_CHANGED, NULL);
	
	row_y += vertical_spacing;
}


static void _gui_tile_settings_setup_units()
{
	// Create the control label
	units_sw_lbl = lv_label_create(tile);
	lv_label_set_long_mode(units_sw_lbl, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_align(units_sw_lbl, LV_TEXT_ALIGN_RIGHT, 0);
	lv_obj_set_style_text_font(units_sw_lbl, &lv_font_montserrat_24, LV_PART_MAIN);
	lv_obj_set_pos(units_sw_lbl, 0, row_y);
	lv_obj_set_width(units_sw_lbl, tile_w/2 - 5);
	lv_label_set_text_static(units_sw_lbl, "Metric");
	
	// Create the units switch
	units_sw = lv_switch_create(tile);
	lv_obj_set_width(units_sw, tile_w/6);
	lv_obj_set_pos(units_sw, tile_w/2 + 5, row_y);
	lv_obj_add_event_cb(units_sw, _gui_tile_settings_sw_cb, LV_EVENT_VALUE_CHANGED, NULL);
	
	row_y += vertical_spacing;
}


static void _gui_tile_settings_setup_save()
{
	uint16_t btn_w = tile_w / 4;
	uint16_t btn_h = tile_h / 10;
	
	// Save button and label
	save_btn = lv_btn_create(tile);
	lv_obj_set_size(save_btn, btn_w, btn_h);
	lv_obj_set_pos(save_btn, (tile_w - btn_w)/2, row_y);
	lv_obj_add_event_cb(save_btn, _gui_tile_settings_btn_cb, LV_EVENT_ALL, NULL);
	
	save_btn_lbl = lv_label_create(save_btn);
	lv_obj_set_style_text_font(save_btn_lbl, &lv_font_montserrat_30, LV_PART_MAIN);
	lv_label_set_text_static(save_btn_lbl, "Save");
	lv_obj_center(save_btn_lbl);
	
	// Informational message below button
	save_btn_info_lbl = lv_label_create(tile);
	lv_label_set_long_mode(save_btn_info_lbl, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_align(save_btn_info_lbl, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_set_width(save_btn_info_lbl, tile_w);
	lv_obj_set_pos(save_btn_info_lbl, 0, row_y + btn_h + 5);
	lv_label_set_text_static(save_btn_info_lbl, "(Reboots Display)");
	
	row_y += vertical_spacing;
}


static void _gui_tile_settings_setup_version()
{
	const esp_app_desc_t* app_desc;
	
	// Get the app version
	app_desc = esp_app_get_description();
	
	// Create the version info label
	version_info_lbl = lv_label_create(tile);
	lv_label_set_long_mode(version_info_lbl, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_align(version_info_lbl, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_set_style_text_font(version_info_lbl, &lv_font_montserrat_18, LV_PART_MAIN);
	lv_obj_set_width(version_info_lbl, tile_w);
	lv_obj_set_pos(version_info_lbl, 0, row_y + vertical_spacing - 40);
	lv_label_set_text(version_info_lbl, app_desc->version);
}


static void _gui_tile_settings_dd_cb(lv_event_t* e)
{
	lv_event_code_t code = lv_event_get_code(e);
	lv_obj_t* obj = lv_event_get_target(e);
	
	if (code == LV_EVENT_VALUE_CHANGED) {
		if (obj == vehicle_dd) {
			lv_dropdown_get_selected_str(obj, new_vehicle_name, PS_VEHICLE_NAME_MAX_LEN);
		} else if (obj == connection_dd) {
			new_if_index = (uint32_t) lv_dropdown_get_selected(obj);
			
			// Display Wifi Screen if necessary to allow configuration
			if (new_if_index == CAN_MANAGER_IF_WIFI) {
				prev_screen_settings = true;
				gui_set_screen_page(GUI_SCREEN_WIFI);
			} else if (new_if_index == CAN_MANAGER_IF_BLE) {
				prev_screen_settings = true;
				gui_set_screen_page(GUI_SCREEN_BLE);
			}
		}
	}
}


static void _gui_tile_settings_sl_cb(lv_event_t* e)
{
	lv_event_code_t code = lv_event_get_code(e);
	lv_obj_t* obj = lv_event_get_target(e);
	
	if (code == LV_EVENT_VALUE_CHANGED) {
		if (obj == brightness_sl) {
			cur_brightness = (uint8_t) lv_slider_get_value(obj);
			
			// Immediately set the new brightness
			disp_driver_set_bl(cur_brightness);
		}
	}
}


static void _gui_tile_settings_sw_cb(lv_event_t* e)
{
	lv_event_code_t code = lv_event_get_code(e);
	lv_obj_t* obj = lv_event_get_target(e);
	
	if (code == LV_EVENT_VALUE_CHANGED) {
		if (obj == units_sw) {
			new_is_metric = lv_obj_has_state(obj, LV_STATE_CHECKED);
		}
	}
}


static void _gui_tile_settings_btn_cb(lv_event_t* e)
{
	bool changed = false;
	
	lv_event_code_t code = lv_event_get_code(e);
	lv_obj_t* obj = lv_event_get_target(e);
	
	if (code == LV_EVENT_CLICKED) {
		if (obj == save_btn) {
			// Look for configuration changes 
			if (strcmp(cur_vehicle_name, new_vehicle_name) != 0) {
				strncpy(configP->vehicle_name, new_vehicle_name, PS_VEHICLE_NAME_MAX_LEN);
				changed = true;
			}
			
			if (cur_if_index != new_if_index) {
				configP->connection_index = new_if_index;
				changed = true;
			}
			
			if (cur_brightness != configP->bl_percent) {
				// Cases:
				//   1. Brightness has been changed while this tile displayed
				//   2. Brightness was changed when this tile was previous displayed, swiped away from and returned back to
				configP->bl_percent = (uint32_t) cur_brightness;
				changed = true;
			}
			
			if (cur_is_metric != new_is_metric) {
				if (new_is_metric) {
					configP->config_flags |= PS_MAIN_FLAG_METRIC;
				} else {
					configP->config_flags &= ~PS_MAIN_FLAG_METRIC;
				}
				changed = true;
			}
			
			// Save changes if necessary
			if (changed) {
				if (ps_save_config(PS_CONFIG_TYPE_MAIN)) {
					ESP_LOGI(TAG, "Updated persistent storage - rebooting");
					vTaskDelay(pdMS_TO_TICKS(50));
				} else {
					ESP_LOGE(TAG, "Could not update persistent storage");
				}
			} else {
				ESP_LOGI(TAG, "No changes detected on Save press");
			}
			
			// Finally reboot
			esp_restart();
		}
	}
}


static void _gui_tile_settings_connection_status_timer_cb(lv_timer_t* timer)
{
	bool new_is_connected;
	
	if (timer == connection_status_eval_timer) {
		new_is_connected = can_connected();
		if (is_connected != new_is_connected) {
			is_connected = new_is_connected;
			if (is_connected) {
				lv_label_set_text_static(connection_status_lbl, LV_SYMBOL_REFRESH);				
			} else {
				lv_label_set_text_static(connection_status_lbl, "");
			}
		}
	}
}
