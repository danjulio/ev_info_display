/*
 * Electrical information display tile.  Display both HV and LV battery voltage
 * and current as well as HV battery min/max temps and LV battery temp (if supported).
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
#include "data_broker.h"
#include "esp_system.h"
#include "gui_task.h"
#include "gui_screen_main.h"
#include "gui_tile_electrical.h"
#include "gui_utilities.h"
#include "vehicle_manager.h"
#include <math.h>
#include <stdio.h>


//
// Local Variables
//
static lv_obj_t* tile;

// HV section
static lv_obj_t* meter_hv_i;
static lv_obj_t* hv_i_pos_arc;
static lv_obj_t* hv_i_neg_arc;
static lv_obj_t* hv_v_val_lbl;
static lv_obj_t* hv_i_val_lbl;
static lv_obj_t* hv_t_val_lbl;

// LV section
static lv_obj_t* meter_lv_v;
static lv_obj_t* lv_v_arc;
static lv_obj_t* lv_v_val_lbl;
static lv_obj_t* lv_i_val_lbl;
static lv_obj_t* lv_t_val_lbl;

static lv_anim_t hv_i_animation;   // Animator for smooth meter movement between values


// Vehicle capability flags
static bool has_hv_v;
static bool has_hv_i;
static bool has_hv_min_t;
static bool has_hv_max_t;
static bool has_lv_v;
static bool has_lv_i;
static bool has_lv_t;

// Vehicle specific ranges
static float hv_i_min;
static float hv_i_max;
static float lv_v_min;
static float lv_v_max;

// State
static bool units_metric;
static uint16_t tile_w;
static uint16_t tile_h;
static int32_t hv_v = 0;
static int32_t hv_i = 0;
static int32_t hv_t_min = 0;
static int32_t hv_t_max = 0;
static float lv_v = 0;
static float lv_i = 0;
static int32_t lv_t = 0;



//
// Forward declarations for internal functions
//
static void _gui_tile_electrical_set_active(bool en);
static void _gui_tile_electrical_setup_vehicle();
static void _gui_tile_electrical_setup_hv_i_meter();
static void _gui_tile_electrical_setup_hv_v_display();
static void _gui_tile_electrical_setup_hv_t_display();
static void _gui_tile_electrical_setup_lv_v_meter();
static void _gui_tile_electrical_setup_lv_i_display();
static void _gui_tile_electrical_setup_lv_t_display();
static void _gui_tile_electrical_update_hv_i_meter(int32_t val, bool immediate);
static void _gui_tile_electrical_set_hv_i_meter_cb(void* indic, int32_t val);
static void _gui_tile_electrical_update_hv_v_display(int32_t val);
static void _gui_tile_electrical_update_hv_t_display(bool has_min, int32_t min, bool has_max, int32_t max);
static void _gui_tile_electrical_update_lv_v_meter(float val);
static void _gui_tile_electrical_update_lv_i_display(float val);
static void _gui_tile_electrical_update_lv_t_display(int32_t val);
static void _gui_tile_electrical_hv_v_cb(float val);
static void _gui_tile_electrical_hv_i_cb(float val);
static void _gui_tile_electrical_hv_min_t_cb(float val);
static void _gui_tile_electrical_hv_max_t_cb(float val);
static void _gui_tile_electrical_lv_v_cb(float val);
static void _gui_tile_electrical_lv_i_cb(float val);
static void _gui_tile_electrical_lv_t_cb(float val);



//
// API
//
void gui_tile_electrical_init(lv_obj_t* parent_tileview, int* tile_index)
{
	// Create our object
	tile = lv_tileview_add_tile(parent_tileview, *tile_index, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
	*tile_index += 1;
	
	gui_get_screen_size(&tile_w, &tile_h);
	
	// Determine our capabilities
	_gui_tile_electrical_setup_vehicle();
	
	// Create display objects
	if (has_hv_i) {
		// Only display HV objects if we can draw the meter (no doubt we'll always have this)
		_gui_tile_electrical_setup_hv_i_meter();
		
		if (has_hv_v) {
			_gui_tile_electrical_setup_hv_v_display();
		}
		
		if (has_hv_min_t || has_hv_max_t) {
			_gui_tile_electrical_setup_hv_t_display();
		}
	}
	
	if (has_lv_v) {
		// Only display LV objects if we can draw the meter
		_gui_tile_electrical_setup_lv_v_meter();
		
		if (has_lv_i) {
			_gui_tile_electrical_setup_lv_i_display();
		}
		
		if (has_lv_t) {
			_gui_tile_electrical_setup_lv_t_display();
		}
	}
	
	// Register ourselves with our parent if we're capable of displaying something
	if (has_hv_i || has_lv_v) {
		gui_screen_main_register_tile(tile, _gui_tile_electrical_set_active);
	}
	
	// Get our display units
	units_metric = gui_is_metric();
}





//
// Internal functions
//
static void _gui_tile_electrical_set_active(bool en)
{
	uint32_t req_mask = 0;
	
	if (en) {
		// Setup to receive data we require
		if (has_hv_i) {
			db_register_gui_callback(DB_ITEM_HV_BATT_I, _gui_tile_electrical_hv_i_cb);
			req_mask |= DB_ITEM_HV_BATT_I;
			hv_v = 0;
			_gui_tile_electrical_update_hv_i_meter(0, true);
			
			if (has_hv_v) {
				db_register_gui_callback(DB_ITEM_HV_BATT_V, _gui_tile_electrical_hv_v_cb);
				req_mask |= DB_ITEM_HV_BATT_V;
				hv_i = 0;
				_gui_tile_electrical_update_hv_v_display(0);
			}
			
			if (has_hv_min_t) {
				db_register_gui_callback(DB_ITEM_HV_BATT_MIN_T, _gui_tile_electrical_hv_min_t_cb);
				req_mask |= DB_ITEM_HV_BATT_MIN_T;
				hv_t_min = 0;
			}
			
			if (has_hv_max_t) {
				db_register_gui_callback(DB_ITEM_HV_BATT_MAX_T, _gui_tile_electrical_hv_max_t_cb);
				req_mask |= DB_ITEM_HV_BATT_MAX_T;
				hv_t_max = 0;
			}
			
			if (has_hv_min_t || has_hv_max_t) {
				_gui_tile_electrical_update_hv_t_display(has_hv_min_t, 0, has_hv_max_t, 0);
			}
		}
		if (has_lv_v) {
			db_register_gui_callback(DB_ITEM_LV_BATT_V, _gui_tile_electrical_lv_v_cb);
			req_mask |= DB_ITEM_LV_BATT_V;
			lv_v = 0;
			_gui_tile_electrical_update_lv_v_meter(0);
			
			if (has_lv_i) {
				db_register_gui_callback(DB_ITEM_LV_BATT_I, _gui_tile_electrical_lv_i_cb);
				req_mask |= DB_ITEM_LV_BATT_I;
				lv_i = 0;
				_gui_tile_electrical_update_lv_i_display(0);
			}
			
			if (has_lv_t) {
				db_register_gui_callback(DB_ITEM_LV_BATT_T, _gui_tile_electrical_lv_t_cb);
				req_mask |= DB_ITEM_LV_BATT_T;
				lv_t = 0;
				_gui_tile_electrical_update_lv_t_display(0);
			}
		}
		if (has_hv_i || has_lv_v) {
			// Start data flow
			vm_set_request_item_mask(req_mask);
		}
		
		// Initialize the update interval timer for meter animations (this will change
		// to reflect real system timing)
		gui_utility_init_update_time(100);
	}
}


static void _gui_tile_electrical_setup_vehicle()
{
	uint32_t capability_mask;
	
	capability_mask = vm_get_supported_item_mask();
	
	has_hv_v     = (capability_mask & DB_ITEM_HV_BATT_V) != 0;
	has_hv_i     = (capability_mask & DB_ITEM_HV_BATT_I) != 0;
	has_hv_min_t = (capability_mask & DB_ITEM_HV_BATT_MIN_T) != 0;
	has_hv_max_t = (capability_mask & DB_ITEM_HV_BATT_MAX_T) != 0;
	has_lv_v     = (capability_mask & DB_ITEM_LV_BATT_V) != 0;
	has_lv_i     = (capability_mask & DB_ITEM_LV_BATT_I) != 0;
	has_lv_t     = (capability_mask & DB_ITEM_LV_BATT_T) != 0;
	
	if (has_hv_i) {
		vm_get_range(VM_RANGE_HV_BATTI, &hv_i_min, &hv_i_max);
	}
	
	if (has_lv_v) {
		vm_get_range(VM_RANGE_LV_BATTV, &lv_v_min, &lv_v_max);
	}
}


static void _gui_tile_electrical_setup_hv_i_meter()
{
	int32_t meter_min = (int32_t) hv_i_min;
	int32_t meter_max = (int32_t) hv_i_max;
	uint16_t meter_ticks = gui_utility_setup_large_270_meter_ticks(hv_i_min, hv_i_max);
	lv_meter_indicator_t * indic;
	
	// Meter background
    meter_hv_i = lv_meter_create(tile);
    lv_obj_center(meter_hv_i);
    lv_obj_set_size(meter_hv_i, tile_w, tile_h);

    // Remove the circle from the middle
    lv_obj_remove_style(meter_hv_i, NULL, LV_PART_INDICATOR);

    // Add a scale first
    lv_meter_scale_t * scale = lv_meter_add_scale(meter_hv_i);
    lv_meter_set_scale_ticks(meter_hv_i, scale, meter_ticks, 2, 20, lv_palette_main(LV_PALETTE_GREY));
    lv_meter_set_scale_major_ticks(meter_hv_i, scale, 2, 3, 30, lv_color_hex3(0xeee), 20);
    lv_meter_set_scale_range(meter_hv_i, scale, meter_min, meter_max, 270, 135);
    lv_obj_set_style_text_font(meter_hv_i, &lv_font_montserrat_18, LV_PART_MAIN);
    
    // Add a blue arc for the negative (regen) part of the meter
	indic = lv_meter_add_arc(meter_hv_i, scale, 5, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_meter_set_indicator_start_value(meter_hv_i, indic, meter_min);
    lv_meter_set_indicator_end_value(meter_hv_i, indic, 0);
    
    // Make the tick lines blue for the negative (regen) part of the meter
    indic = lv_meter_add_scale_lines(meter_hv_i, scale, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_BLUE), false, 0);
    lv_meter_set_indicator_start_value(meter_hv_i, indic, meter_min);
    lv_meter_set_indicator_end_value(meter_hv_i, indic, 0);

	// Create a green arc that will act as the positive meter indicator (traction current)
	hv_i_pos_arc = lv_arc_create(tile);
	lv_obj_center(hv_i_pos_arc);
	lv_obj_set_size(hv_i_pos_arc, tile_w-10, tile_h-10);
	lv_arc_set_rotation(hv_i_pos_arc, 135 + (270 * (-meter_min) / (meter_max - meter_min)));
	lv_arc_set_bg_angles(hv_i_pos_arc, 0, 270 - (270 * (-meter_min) / (meter_max - meter_min)));
	lv_arc_set_range(hv_i_pos_arc, 0, meter_max);
	lv_arc_set_value(hv_i_pos_arc, 0);
	lv_obj_set_style_bg_color(hv_i_pos_arc, lv_palette_main(LV_PALETTE_BLUE_GREY), LV_PART_INDICATOR);
	lv_obj_remove_style(hv_i_pos_arc, NULL, LV_PART_KNOB);
	lv_obj_clear_flag(hv_i_pos_arc, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_set_style_arc_color(hv_i_pos_arc, lv_palette_main(LV_PALETTE_GREEN), LV_PART_INDICATOR);
	
	// Create a blue arc that will act as the negative meter indicator (regen current)
	hv_i_neg_arc = lv_arc_create(tile);
	lv_obj_center(hv_i_neg_arc);
	lv_obj_set_size(hv_i_neg_arc, tile_w-10, tile_h-10);
	lv_arc_set_rotation(hv_i_neg_arc, 135);
	lv_arc_set_bg_angles(hv_i_neg_arc, 0, 270 * (-meter_min) / (meter_max - meter_min));
	lv_arc_set_range(hv_i_neg_arc, 0, -meter_min);
	lv_arc_set_value(hv_i_neg_arc, 0);
	lv_obj_set_style_bg_color(hv_i_neg_arc, lv_palette_main(LV_PALETTE_BLUE_GREY), LV_PART_INDICATOR);
	lv_obj_remove_style(hv_i_neg_arc, NULL, LV_PART_KNOB);
	lv_obj_clear_flag(hv_i_neg_arc, LV_OBJ_FLAG_CLICKABLE);
	lv_arc_set_mode(hv_i_neg_arc, LV_ARC_MODE_REVERSE);
	
	// Add the label object for the current value
	hv_i_val_lbl = lv_label_create(tile);
	lv_label_set_long_mode(hv_i_val_lbl, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_align(hv_i_val_lbl, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_set_style_text_font(hv_i_val_lbl, &lv_font_montserrat_30, LV_PART_MAIN);
	lv_obj_align(hv_i_val_lbl, LV_ALIGN_CENTER, 0, -(tile_h/4) + 10);
	
	// Initialize the meter to 0
	hv_i = -1.0;  // Force update
	_gui_tile_electrical_update_hv_i_meter(0, true);
}


static void _gui_tile_electrical_setup_hv_v_display()
{
	// Create the label object for the current voltage value
	hv_v_val_lbl = lv_label_create(tile);
	lv_label_set_long_mode(hv_v_val_lbl, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_align(hv_v_val_lbl, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_set_style_text_font(hv_v_val_lbl, &lv_font_montserrat_48, LV_PART_MAIN);
	lv_obj_align(hv_v_val_lbl, LV_ALIGN_CENTER, 0, -(tile_h/4) + 60);
	lv_label_set_text_static(hv_v_val_lbl, "");  // Blank initially
}


static void _gui_tile_electrical_setup_hv_t_display()
{
	// Create the label object for the current temperature value
	hv_t_val_lbl = lv_label_create(tile);
	lv_label_set_long_mode(hv_t_val_lbl, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_align(hv_t_val_lbl, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_set_style_text_font(hv_t_val_lbl, &lv_font_montserrat_30, LV_PART_MAIN);
	lv_obj_align(hv_t_val_lbl, LV_ALIGN_CENTER, 0, -(tile_h/4) + 105);
	lv_label_set_text_static(hv_t_val_lbl, "");  // Blank initially
}


static void _gui_tile_electrical_setup_lv_v_meter()
{
	uint16_t w = 6*tile_w/16;
	uint16_t h = 6*tile_h/16;
	int32_t meter_min = (int32_t) lv_v_min;
	int32_t meter_max = (int32_t) lv_v_max;
	uint16_t meter_ticks = gui_utility_setup_small_180_meter_ticks(lv_v_min, lv_v_max);
	
	// Meter
	meter_lv_v = lv_meter_create(tile);
    lv_obj_align(meter_lv_v, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_size(meter_lv_v, w, h);
    lv_obj_remove_style(meter_lv_v, NULL, LV_PART_INDICATOR);
    
    // Meter scale (V)
    lv_meter_scale_t* scale = lv_meter_add_scale(meter_lv_v);
    lv_obj_set_style_border_color(meter_lv_v, lv_palette_main(LV_PALETTE_BLUE_GREY), LV_PART_MAIN);
    lv_meter_set_scale_ticks(meter_lv_v, scale, meter_ticks, 3, 6, lv_palette_main(LV_PALETTE_GREY));
    lv_meter_set_scale_major_ticks(meter_lv_v, scale, 2, 3, 10, lv_color_hex3(0xeee), 10);
    lv_meter_set_scale_range(meter_lv_v, scale, meter_min, meter_max, 180, 180);

    // Green Meter arc (V * 10)
    lv_v_arc = lv_arc_create(tile);
    lv_obj_align(lv_v_arc, LV_ALIGN_BOTTOM_MID, 0, -25);
	lv_obj_set_size(lv_v_arc, w-10, h-10);
    lv_arc_set_rotation(lv_v_arc, 180);
	lv_arc_set_bg_angles(lv_v_arc, 0, 180);
	lv_arc_set_range(lv_v_arc, meter_min * 10, meter_max * 10);
	lv_obj_set_style_bg_color(lv_v_arc, lv_palette_main(LV_PALETTE_BLUE_GREY), LV_PART_INDICATOR);
	lv_obj_remove_style(lv_v_arc, NULL, LV_PART_KNOB);
	lv_obj_clear_flag(lv_v_arc, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_set_style_arc_color(lv_v_arc, lv_palette_main(LV_PALETTE_GREEN), LV_PART_INDICATOR);
	
	// Value label
	lv_v_val_lbl = lv_label_create(tile);
	lv_label_set_long_mode(lv_v_val_lbl, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_align(lv_v_val_lbl, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_set_style_text_font(lv_v_val_lbl, &lv_font_montserrat_24, LV_PART_MAIN);
	lv_obj_align(lv_v_val_lbl, LV_ALIGN_BOTTOM_MID, 0, -h/2 - 20);
	
	// Initialize meter to 0
	_gui_tile_electrical_update_lv_v_meter(0);
}


static void _gui_tile_electrical_setup_lv_i_display()
{
	uint16_t h = 6*tile_h/16;
	
	// Create the label object for the current voltage value
	lv_i_val_lbl = lv_label_create(tile);
	lv_label_set_long_mode(lv_i_val_lbl, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_align(lv_i_val_lbl, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_set_style_text_font(lv_i_val_lbl, &lv_font_montserrat_24, LV_PART_MAIN);
	lv_obj_align(lv_i_val_lbl, LV_ALIGN_BOTTOM_MID, 0, -h/2 + 10);
	lv_label_set_text_static(lv_i_val_lbl, "");  // Blank initially
}


static void _gui_tile_electrical_setup_lv_t_display()
{
	uint16_t h = 6*tile_h/16;
	
	// Create the label object for the current temperature value
	lv_t_val_lbl = lv_label_create(tile);
	lv_label_set_long_mode(lv_t_val_lbl, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_align(lv_t_val_lbl, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_set_style_text_font(lv_t_val_lbl, &lv_font_montserrat_24, LV_PART_MAIN);
	lv_obj_align(lv_t_val_lbl, LV_ALIGN_BOTTOM_MID, 0, -h/2 + 40);
	lv_label_set_text_static(lv_t_val_lbl, "");  // Blank initially
}


static void _gui_tile_electrical_update_hv_i_meter(int32_t val, bool immediate)
{
	static char hv_i_lbl[16];             // "-XXXX A"
	uint32_t anim_time;
	
	// Update the label immediately
	sprintf(hv_i_lbl, "%ld A", val);
	lv_label_set_text_static(hv_i_val_lbl, hv_i_lbl);
	
	if (immediate) {
		_gui_tile_electrical_set_hv_i_meter_cb(NULL, val);
	} else {
		// Start an animation to the new value for the meter indicator
		anim_time = gui_utility_get_update_period() - 20;   // Just slightly faster than the average update interval
		lv_anim_init(&hv_i_animation);
		lv_anim_set_exec_cb(&hv_i_animation, _gui_tile_electrical_set_hv_i_meter_cb);
		lv_anim_set_var(&hv_i_animation, meter_hv_i); // Unused since we will update the arcs
		lv_anim_set_time(&hv_i_animation, anim_time);
		lv_anim_set_values(&hv_i_animation, hv_i, val);
		lv_anim_start(&hv_i_animation);
	}
}


static void _gui_tile_electrical_set_hv_i_meter_cb(void* indic, int32_t val)
{
	if (val < 0) {
		lv_arc_set_value(hv_i_pos_arc, 0);
		lv_arc_set_value(hv_i_neg_arc, ((int32_t) round(-hv_i_min)) + val);
	} else {
		lv_arc_set_value(hv_i_neg_arc, (int32_t) round(-hv_i_min));
		lv_arc_set_value(hv_i_pos_arc, val);
	}
}


static void _gui_tile_electrical_update_hv_v_display(int32_t val)
{
	static char hv_v_lbl[16];             // "XXX V"
	
	sprintf(hv_v_lbl, "%ld V",  val);
	lv_label_set_text_static(hv_v_val_lbl, hv_v_lbl);
}


static void _gui_tile_electrical_update_hv_t_display(bool has_min, int32_t min, bool has_max, int32_t max)
{
	static char hv_t_lbl[32];            // "-XX °C / -XX °C"
	int len;
	
	hv_t_lbl[0] = 0;
	
	if (has_min) {
		sprintf(hv_t_lbl, "%ld", min);
	}
	
	if (has_min && has_max) {
		len = strlen(hv_t_lbl);
		sprintf(&hv_t_lbl[len], " / ");
	}
	
	if (has_max) {
		len = strlen(hv_t_lbl);
		sprintf(&hv_t_lbl[len], "%ld", max);
	}
	
	len = strlen(hv_t_lbl);
	if (units_metric) {
		sprintf(&hv_t_lbl[len], " °C");
	} else {
		sprintf(&hv_t_lbl[len], " °F");
	}
	
	lv_label_set_text_static(hv_t_val_lbl, hv_t_lbl);
}


static void _gui_tile_electrical_update_lv_v_meter(float val)
{
	static char lv_v_lbl[16];             // "XXX V"
	int32_t arc_val;
	
	arc_val = (int32_t) round(val * 10.0);
	lv_arc_set_value(lv_v_arc, arc_val);
	
	sprintf(lv_v_lbl, "%1.1f V", val);
	lv_label_set_text_static(lv_v_val_lbl, lv_v_lbl);
}


static void _gui_tile_electrical_update_lv_i_display(float val)
{
	static char lv_i_lbl[16];             // "XXX.X A"
	
	sprintf(lv_i_lbl, "%1.1f A",  val);
	lv_label_set_text_static(lv_i_val_lbl, lv_i_lbl);
}


static void _gui_tile_electrical_update_lv_t_display(int32_t val)
{
	static char lv_t_lbl[16];             // "-XX °C"
	
	if (units_metric) {
		sprintf(lv_t_lbl, "%ld °C",  val);
	} else {
		sprintf(lv_t_lbl, "%ld °F",  val);
	}
	lv_label_set_text_static(lv_t_val_lbl, lv_t_lbl);
}


static void _gui_tile_electrical_hv_v_cb(float val)
{
	int32_t v;
	
	v = round(val);
	
	if (v != hv_v) {
		_gui_tile_electrical_update_hv_v_display(v);
		hv_v = v;
	}
}


// Note: Since we are displaying this information in terms of traction on the meter
// we need to negate the incoming current reading since it is from the perspective
// of the battery.
static void _gui_tile_electrical_hv_i_cb(float val)
{
	int32_t i;
	
	// Use this update to mark the intervals for the update timer
	gui_utility_note_update();
	
	// Negate current since we want to display a positive number for traction
	// and a negative number for regeneration (battery current is negative for traction)
	i = round(-val);
	
	if (i != hv_i) {
		_gui_tile_electrical_update_hv_i_meter(i, false);
		hv_i = i;
	}
}


static void _gui_tile_electrical_hv_min_t_cb(float val)
{
	int32_t t;
	
	t = round((units_metric) ? val : gui_util_c_to_f(val));
	
	if (t != hv_t_min) {
		_gui_tile_electrical_update_hv_t_display(has_hv_min_t, t, has_hv_max_t, hv_t_max);
		hv_t_min = t;
	}
}


static void _gui_tile_electrical_hv_max_t_cb(float val)
{
	int32_t t;
	
	t = round((units_metric) ? val : gui_util_c_to_f(val));
	
	if (t != hv_t_max) {
		_gui_tile_electrical_update_hv_t_display(has_hv_min_t, hv_t_min, has_hv_max_t, t);
		hv_t_max = t;
	}
}


static void _gui_tile_electrical_lv_v_cb(float val)
{
	if (val != lv_v) {
		_gui_tile_electrical_update_lv_v_meter(val);
		lv_v = val;
	}
}


static void _gui_tile_electrical_lv_i_cb(float val)
{
	if (val != lv_i) {
		_gui_tile_electrical_update_lv_i_display(val);
		lv_i = val;
	}
}


static void _gui_tile_electrical_lv_t_cb(float val)
{
	int32_t t;
	
	t = round((units_metric) ? val : gui_util_c_to_f(val));
	
	if (t != lv_t) {
		_gui_tile_electrical_update_lv_t_display(t);
		lv_t = t;
	}
}
