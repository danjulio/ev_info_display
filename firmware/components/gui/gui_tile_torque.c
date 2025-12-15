/*
 * Torque display tile.  Display torque values (front and/or rear), speed and 
 * elevation.
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
#include "gui_tile_torque.h"
#include "gui_utilities.h"
#include "vehicle_manager.h"
#include <math.h>
#include <stdio.h>



//
// Local constants
//
#define FRONT_TORQUE 0
#define REAR_TORQUE  1



//
// Local Variables
//
static lv_obj_t* tile;

static lv_obj_t* meter_torque;
static lv_obj_t* f_torque_pos_arc;
static lv_obj_t* f_torque_neg_arc;
static lv_obj_t* r_torque_pos_arc;
static lv_obj_t* r_torque_neg_arc;
static lv_obj_t* torque_val_lbl;

static lv_obj_t* speed_val_lbl;

static lv_obj_t* elevation_val_lbl;

static lv_anim_t torque_animation[2];

// Vehicle capability flags
static bool has_torque[2];
static bool has_speed;
static bool has_elevation;

// Vehicle specific ranges
static float torque_min;
static float torque_max;

// State
static bool units_metric;
static uint16_t tile_w;
static uint16_t tile_h;
static int32_t torque[2];            // N-m
static int32_t speed;                // KPH or MPH
static int32_t elevation;            // Meters or Feet



//
// Forward declarations for internal functions
//
static void _gui_tile_torque_set_active(bool en);
static void _gui_tile_torque_setup_vehicle();
static void _gui_tile_torque_setup_torque_meter();
static void _gui_tile_torque_setup_speed_display();
static void _gui_tile_torque_setup_elevation_display();
static void _gui_tile_torque_update_torque_meter(int32_t val, int index, bool immediate);
static void _gui_tile_torque_set_torque_meter_cb(void* indic, int32_t val);
static void _gui_tile_torque_update_speed_display(int32_t val);
static void _gui_tile_torque_update_elevation_display(int32_t val);
static void _gui_tile_torque_front_cb(float val);
static void _gui_tile_torque_rear_cb(float val);
static void _gui_tile_torque_speed_cb(float val);
static void _gui_tile_torque_elevation_cb(float val);


void gui_tile_torque_init(lv_obj_t* parent_tileview, int* tile_index)
{
	// Create our object
	tile = lv_tileview_add_tile(parent_tileview, *tile_index, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
	*tile_index += 1;
	
	gui_get_screen_size(&tile_w, &tile_h);
	
	// Determine our capabilities
	_gui_tile_torque_setup_vehicle();
	
	// Create display objects
	if (has_torque[FRONT_TORQUE] || has_torque[REAR_TORQUE]) {
		_gui_tile_torque_setup_torque_meter();
	}
	
	if (has_speed) {
		_gui_tile_torque_setup_speed_display();
	}
	
	if (has_elevation) {
		_gui_tile_torque_setup_elevation_display();
	}
	
	// Register ourselves with our parent if we're capable of displaying something
	if (has_torque[FRONT_TORQUE] || has_torque[REAR_TORQUE]) {
		gui_screen_main_register_tile(tile, _gui_tile_torque_set_active);
	}
	
	// Get our display units
	units_metric = gui_is_metric();
}



//
// Internal functions
//
static void _gui_tile_torque_set_active(bool en)
{
	uint32_t req_mask = 0;
	
	if (en) {
		if (has_torque[FRONT_TORQUE]) {
			db_register_gui_callback(DB_ITEM_FRONT_TORQUE, _gui_tile_torque_front_cb);
			req_mask |= DB_ITEM_FRONT_TORQUE;
			torque[FRONT_TORQUE] = 0;
			_gui_tile_torque_update_torque_meter(0, FRONT_TORQUE, true);
		}
		
		if (has_torque[REAR_TORQUE]) {
			db_register_gui_callback(DB_ITEM_REAR_TORQUE, _gui_tile_torque_rear_cb);
			req_mask |= DB_ITEM_REAR_TORQUE;
			torque[REAR_TORQUE] = 0;
			_gui_tile_torque_update_torque_meter(0, REAR_TORQUE, true);
		}
		
		if (has_speed) {
			db_register_gui_callback(DB_ITEM_SPEED, _gui_tile_torque_speed_cb);
			req_mask |= DB_ITEM_SPEED;
			speed = 0;
			_gui_tile_torque_update_speed_display(0);
		}
		
		if (has_elevation) {
			db_register_gui_callback(DB_ITEM_GPS_ELEVATION, _gui_tile_torque_elevation_cb);
			req_mask |= DB_ITEM_GPS_ELEVATION;
			elevation = 0;
			_gui_tile_torque_update_elevation_display(0);
		}
		
		if (has_torque[FRONT_TORQUE] || has_torque[REAR_TORQUE] || has_speed || has_elevation) {
			// Start data flow
			vm_set_request_item_mask(req_mask);
		}
		
		// Enable averaging for incoming data if we have a fast enough interface
		db_enable_fast_average(gui_has_fast_interface());
		
		// Initialize the update interval timer for meter animations (this will change
		// to reflect real system timing)
		gui_utility_init_update_time(100);
	}
}


static void _gui_tile_torque_setup_vehicle()
{
	uint32_t capability_mask;
	
	capability_mask = vm_get_supported_item_mask();
	
	has_torque[FRONT_TORQUE] = (capability_mask & DB_ITEM_FRONT_TORQUE) != 0;
	has_torque[REAR_TORQUE]  = (capability_mask & DB_ITEM_REAR_TORQUE) != 0;
	has_speed                = (capability_mask & DB_ITEM_SPEED) != 0;
	has_elevation            = (capability_mask & DB_ITEM_GPS_ELEVATION) != 0;
	
	if (has_torque[FRONT_TORQUE] || has_torque[REAR_TORQUE]) {
		vm_get_range(VM_RANGE_TORQUE, &torque_min, &torque_max);
	}
}


static void _gui_tile_torque_setup_torque_meter()
{
	int32_t meter_min = (int32_t) torque_min;
	int32_t meter_max = (int32_t) torque_max;
	uint16_t meter_ticks = gui_utility_setup_large_270_meter_ticks(torque_min, torque_max);
	uint16_t arc_inset = 12;
	lv_meter_indicator_t * indic;
	
	// Meter background
    meter_torque = lv_meter_create(tile);
    lv_obj_center(meter_torque);
    if (has_torque[FRONT_TORQUE] && has_torque[REAR_TORQUE]) {
    	// Add extra space for second arc
    	lv_obj_set_size(meter_torque, tile_w-(2*arc_inset), tile_h-(2*arc_inset));
    } else {
    	// Use meter's padding for arc
    	lv_obj_set_size(meter_torque, tile_w, tile_h);
    }

    // Remove the circle from the middle
    lv_obj_remove_style(meter_torque, NULL, LV_PART_INDICATOR);

    // Add a scale first
    lv_meter_scale_t * scale = lv_meter_add_scale(meter_torque);
    lv_meter_set_scale_ticks(meter_torque, scale, meter_ticks, 2, 20, lv_palette_main(LV_PALETTE_GREY));
    lv_meter_set_scale_major_ticks(meter_torque, scale, 2, 3, 30, lv_color_hex3(0xeee), 20);
    lv_meter_set_scale_range(meter_torque, scale, meter_min, meter_max, 270, 135);
    lv_obj_set_style_text_font(meter_torque, &lv_font_montserrat_18, LV_PART_MAIN);
    
    // Add a blue arc for the negative (regen) part of the meter
	indic = lv_meter_add_arc(meter_torque, scale, 5, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_meter_set_indicator_start_value(meter_torque, indic, meter_min);
    lv_meter_set_indicator_end_value(meter_torque, indic, 0);
    
    // Make the tick lines blue for the negative (regen) part of the meter
    indic = lv_meter_add_scale_lines(meter_torque, scale, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_BLUE), false, 0);
    lv_meter_set_indicator_start_value(meter_torque, indic, meter_min);
    lv_meter_set_indicator_end_value(meter_torque, indic, 0);

	// Rear torque is outside if both present
	if (has_torque[REAR_TORQUE]) {
		// Create a green arc that will act as the positive meter indicator (traction torque)
		r_torque_pos_arc = lv_arc_create(tile);
		lv_obj_center(r_torque_pos_arc);
		lv_obj_set_size(r_torque_pos_arc, tile_w-arc_inset, tile_h-arc_inset);
		lv_arc_set_rotation(r_torque_pos_arc, 135 + (270 * (-meter_min) / (meter_max - meter_min)));
		lv_arc_set_bg_angles(r_torque_pos_arc, 0, 270 - (270 * (-meter_min) / (meter_max - meter_min)));
		lv_arc_set_range(r_torque_pos_arc, 0, meter_max);
		lv_arc_set_value(r_torque_pos_arc, 0);
		lv_obj_set_style_bg_color(r_torque_pos_arc, lv_palette_main(LV_PALETTE_BLUE_GREY), LV_PART_INDICATOR);
		lv_obj_remove_style(r_torque_pos_arc, NULL, LV_PART_KNOB);
		lv_obj_clear_flag(r_torque_pos_arc, LV_OBJ_FLAG_CLICKABLE);
		lv_obj_set_style_arc_color(r_torque_pos_arc, lv_palette_main(LV_PALETTE_GREEN), LV_PART_INDICATOR);
		
		// Create a blue arc that will act as the negative meter indicator (regen torque)
		r_torque_neg_arc = lv_arc_create(tile);
		lv_obj_center(r_torque_neg_arc);
		lv_obj_set_size(r_torque_neg_arc, tile_w-arc_inset, tile_h-arc_inset);
		lv_arc_set_rotation(r_torque_neg_arc, 135);
		lv_arc_set_bg_angles(r_torque_neg_arc, 0, 270 * (-meter_min) / (meter_max - meter_min));
		lv_arc_set_range(r_torque_neg_arc, 0, -meter_min);
		lv_arc_set_value(r_torque_neg_arc, 0);
		lv_obj_set_style_bg_color(r_torque_neg_arc, lv_palette_main(LV_PALETTE_BLUE_GREY), LV_PART_INDICATOR);
		lv_obj_remove_style(r_torque_neg_arc, NULL, LV_PART_KNOB);
		lv_obj_clear_flag(r_torque_neg_arc, LV_OBJ_FLAG_CLICKABLE);
		lv_arc_set_mode(r_torque_neg_arc, LV_ARC_MODE_REVERSE);
		
		arc_inset += 2*arc_inset;
	}
	
	if (has_torque[FRONT_TORQUE]) {
		// Create a teal arc that will act as the positive meter indicator (traction torque)
		f_torque_pos_arc = lv_arc_create(tile);
		lv_obj_center(f_torque_pos_arc);
		lv_obj_set_size(f_torque_pos_arc, tile_w-arc_inset, tile_h-arc_inset);
		lv_arc_set_rotation(f_torque_pos_arc, 135 + (270 * (-meter_min) / (meter_max - meter_min)));
		lv_arc_set_bg_angles(f_torque_pos_arc, 0, 270 - (270 * (-meter_min) / (meter_max - meter_min)));
		lv_arc_set_range(f_torque_pos_arc, 0, meter_max);
		lv_arc_set_value(f_torque_pos_arc, 0);
		lv_obj_set_style_bg_color(f_torque_pos_arc, lv_palette_main(LV_PALETTE_BLUE_GREY), LV_PART_INDICATOR);
		lv_obj_remove_style(f_torque_pos_arc, NULL, LV_PART_KNOB);
		lv_obj_clear_flag(f_torque_pos_arc, LV_OBJ_FLAG_CLICKABLE);
		lv_obj_set_style_arc_color(f_torque_pos_arc, lv_palette_main(LV_PALETTE_TEAL), LV_PART_INDICATOR);
		
		// Create a light blue arc that will act as the negative meter indicator (regen torque)
		f_torque_neg_arc = lv_arc_create(tile);
		lv_obj_center(f_torque_neg_arc);
		lv_obj_set_size(f_torque_neg_arc, tile_w-arc_inset, tile_h-arc_inset);
		lv_arc_set_rotation(f_torque_neg_arc, 135);
		lv_arc_set_bg_angles(f_torque_neg_arc, 0, 270 * (-meter_min) / (meter_max - meter_min));
		lv_arc_set_range(f_torque_neg_arc, 0, -meter_min);
		lv_arc_set_value(f_torque_neg_arc, 0);
		lv_obj_set_style_bg_color(f_torque_neg_arc, lv_palette_main(LV_PALETTE_BLUE_GREY), LV_PART_INDICATOR);
		lv_obj_remove_style(f_torque_neg_arc, NULL, LV_PART_KNOB);
		lv_obj_clear_flag(f_torque_neg_arc, LV_OBJ_FLAG_CLICKABLE);
		lv_arc_set_mode(f_torque_neg_arc, LV_ARC_MODE_REVERSE);
		lv_obj_set_style_arc_color(f_torque_neg_arc, lv_palette_main(LV_PALETTE_LIGHT_BLUE), LV_PART_INDICATOR);
	}
	
	// Add the label object for the current value
	torque_val_lbl = lv_label_create(tile);
	lv_label_set_long_mode(torque_val_lbl, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_align(torque_val_lbl, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_set_style_text_font(torque_val_lbl, &lv_font_montserrat_30, LV_PART_MAIN);
	lv_obj_align(torque_val_lbl, LV_ALIGN_CENTER, 0, -(tile_h/4) + 10);
	
	// Initialize the meter to 0
	for (int i=0; i<2; i++) {
		torque[i] = 0;
		if (has_torque[i]) {
			_gui_tile_torque_update_torque_meter(0, i, true);
		}
	}
}


static void _gui_tile_torque_setup_speed_display()
{
	// Create the label object for the current voltage value
	speed_val_lbl = lv_label_create(tile);
	lv_label_set_long_mode(speed_val_lbl, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_align(speed_val_lbl, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_set_style_text_font(speed_val_lbl, &lv_font_montserrat_48, LV_PART_MAIN);
	lv_obj_align(speed_val_lbl, LV_ALIGN_CENTER, 0, -40);
	lv_label_set_text_static(speed_val_lbl, "");  // Blank initially
}


static void _gui_tile_torque_setup_elevation_display()
{
	// Create the label object for the current temperature value
	elevation_val_lbl = lv_label_create(tile);
	lv_label_set_long_mode(elevation_val_lbl, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_align(elevation_val_lbl, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_set_style_text_font(elevation_val_lbl, &lv_font_montserrat_24, LV_PART_MAIN);
	lv_obj_align(elevation_val_lbl, LV_ALIGN_BOTTOM_MID, 0, -60);
	lv_label_set_text_static(elevation_val_lbl, "");  // Blank initially
}


static void _gui_tile_torque_update_torque_meter(int32_t val, int index, bool immediate)
{
	static char torque_lbl[16];             // "-XXX N-m"
	static int32_t torque_total = -1;       // Force update on first call when creating the meter
	int32_t new_total = 0;
	uint32_t anim_time;
	
	// Calculate the new total torque
	for (int i=0; i<2; i++) {
		if (i == index) {
			// Use the updated value
			new_total += val;
		} else {
			// Use the existing value for the other torque
			if (has_torque[i]) {
				new_total += torque[i];
			}
		}
	}
	
	if (new_total != torque_total) {
		// Update the label immediately
		sprintf(torque_lbl, "%ld N-m", new_total);
		lv_label_set_text_static(torque_val_lbl, torque_lbl);
		torque_total = new_total;
	}
	
	if (immediate) {
		_gui_tile_torque_set_torque_meter_cb((index == FRONT_TORQUE) ? f_torque_pos_arc : r_torque_pos_arc, val);
	} else {
		// Stop any previous animations
		lv_anim_del((index == FRONT_TORQUE) ? f_torque_pos_arc : r_torque_pos_arc, _gui_tile_torque_set_torque_meter_cb);
		
		// Start an animation to the new value for the meter indicator
		anim_time = gui_utility_get_update_period() - 20;   // Just slightly faster than the average update interval
		lv_anim_init(&torque_animation[index]);
		lv_anim_set_exec_cb(&torque_animation[index], _gui_tile_torque_set_torque_meter_cb);
		lv_anim_set_var(&torque_animation[index], (index == FRONT_TORQUE) ? f_torque_pos_arc : r_torque_pos_arc);
		lv_anim_set_time(&torque_animation[index], anim_time);
		lv_anim_set_values(&torque_animation[index], torque[index], val);
		lv_anim_start(&torque_animation[index]);
	}
}


// index points to either f_torque_pos_arc or r_torque_pos_arc for identification
static void _gui_tile_torque_set_torque_meter_cb(void* indic, int32_t val)
{
	lv_obj_t* pos_arc = indic;
	lv_obj_t* neg_arc = (indic == f_torque_pos_arc) ? f_torque_neg_arc : r_torque_neg_arc;
	
	if (val < 0) {
		lv_arc_set_value(pos_arc, 0);
		lv_arc_set_value(neg_arc, ((int32_t) round(-torque_min)) + val);
	} else {
		lv_arc_set_value(neg_arc, (int32_t) round(-torque_min));
		lv_arc_set_value(pos_arc, val);
	}
}


static void _gui_tile_torque_update_speed_display(int32_t val)
{
	static char speed_lbl[16];             // "XXX km/h"
	
	if (units_metric) {
		sprintf(speed_lbl, "%ld km/h",  val);
	} else {
		sprintf(speed_lbl, "%ld mph",  val);
	}
	lv_label_set_text_static(speed_val_lbl, speed_lbl);
}


static void _gui_tile_torque_update_elevation_display(int32_t val)
{
	static char elevation_lbl[16];             // "XXXXX m" or "XXXXX'"
	
	if (units_metric) {
		sprintf(elevation_lbl, "%ld m",  val);
	} else {
		sprintf(elevation_lbl, "%ld'",  val);
	}
	lv_label_set_text_static(elevation_val_lbl, elevation_lbl);
}


static void _gui_tile_torque_front_cb(float val)
{
	int32_t t;
	
	if (!has_torque[REAR_TORQUE]) {
		// Use this update to mark the intervals for the update timer if we don't have rear torque
		gui_utility_note_update();
	}
	
	t = round(val);
	
	if (t != torque[FRONT_TORQUE]) {
		_gui_tile_torque_update_torque_meter(t, FRONT_TORQUE, false);
		torque[FRONT_TORQUE] = t;
	}
}


static void _gui_tile_torque_rear_cb(float val)
{
	int32_t t;
	
	// Use this update to mark the intervals for the update timer when we have rear torque
	gui_utility_note_update();
	
	t = round(val);
	
	if (t != torque[REAR_TORQUE]) {
		_gui_tile_torque_update_torque_meter(t, REAR_TORQUE, false);
		torque[REAR_TORQUE] = t;
	}
}


static void _gui_tile_torque_speed_cb(float val)
{
	int32_t s;
	
	s = round((units_metric) ? val : gui_util_kph_to_mph(val));
	
	if (s != speed) {
		_gui_tile_torque_update_speed_display(s);
		speed = s;
	}
}


static void _gui_tile_torque_elevation_cb(float val)
{
	int32_t e;
	
	e = round((units_metric) ? val : gui_util_m_to_feet(val));
	
	if (e != elevation) {
		_gui_tile_torque_update_elevation_display(e);
		elevation = e;
	}
}
