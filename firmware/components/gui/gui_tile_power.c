/*
 * Power display tile.  Display power consumption or regeneration in kW as well as
 * auxiliary systems power consumption.
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
#include "gui_tile_power.h"
#include "gui_utilities.h"
#include "vehicle_manager.h"
#include <math.h>
#include <stdio.h>



//
// Local Variables
//
static lv_obj_t* tile;

static lv_obj_t* meter_power;
static lv_obj_t* power_pos_arc;
static lv_obj_t* power_neg_arc;
static lv_obj_t* power_val_lbl;

static lv_obj_t* meter_aux;
static lv_obj_t* aux_arc;
static lv_obj_t* aux_val_lbl;
static lv_obj_t* aux_lbl;

static lv_anim_t power_animation;   // Animator for smooth meter movement between values

// Vehicle capability flags
static bool has_power;
static bool has_aux;

// Vehicle specific ranges
static float power_min;
static float power_max;
static float aux_min;
static float aux_max;

// State
static uint16_t tile_w;
static uint16_t tile_h;
static float hv_v = 0;
static int32_t power_kw = 0;
static float aux_kw = 0;


//
// Forward declarations for internal functions
//
static void _gui_tile_power_set_active(bool en);
static void _gui_tile_power_setup_vehicle();
static void _gui_tile_power_setup_power_meter();
static void _gui_tile_power_setup_aux_meter();
static void _gui_tile_power_update_power_meter(int32_t val, bool immediate);
static void _gui_tile_power_set_power_meter_cb(void* indic, int32_t val);
static void _gui_tile_power_update_aux_meter(float val);
static void _gui_tile_power_hv_v_cb(float val);
static void _gui_tile_power_hv_i_cb(float val);
static void _gui_tile_power_aux_cb(float val);


//
// API
//
void gui_tile_power_init(lv_obj_t* parent_tileview, int* tile_index)
{
	// Create our object
	tile = lv_tileview_add_tile(parent_tileview, *tile_index, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
	*tile_index += 1;
	
	gui_get_screen_size(&tile_w, &tile_h);
	
	// Determine our capabilities
	_gui_tile_power_setup_vehicle();
	
	// Create display objects
	if (has_power) {
		_gui_tile_power_setup_power_meter();
	}
	
	if (has_aux) {
		_gui_tile_power_setup_aux_meter();
	}
	
	// Register ourselves with our parent if we're capable of displaying something
	if (has_power || has_aux) {
		gui_screen_main_register_tile(tile, _gui_tile_power_set_active);
	}
}



//
// Internal functions
//
static void _gui_tile_power_set_active(bool en)
{
	uint32_t req_mask = 0;
	
	if (en) {
		// Setup to receive data we require
		if (has_power) {
			db_register_gui_callback(DB_ITEM_HV_BATT_V, _gui_tile_power_hv_v_cb);
			db_register_gui_callback(DB_ITEM_HV_BATT_I, _gui_tile_power_hv_i_cb);
			req_mask |= DB_ITEM_HV_BATT_V | DB_ITEM_HV_BATT_I;
			hv_v = 0;
			power_kw = 0;
			_gui_tile_power_update_power_meter(0, true);
		}
		if (has_aux) {
			db_register_gui_callback(DB_ITEM_AUX_KW, _gui_tile_power_aux_cb);
			req_mask |= DB_ITEM_AUX_KW;
			aux_kw = 0;
			_gui_tile_power_update_aux_meter(0);
		}
		if (has_power || has_aux) {
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


static void _gui_tile_power_setup_vehicle()
{
	uint32_t capability_mask;
	
	capability_mask = vm_get_supported_item_mask();
	has_power = ((capability_mask & DB_ITEM_HV_BATT_V) != 0) && ((capability_mask & DB_ITEM_HV_BATT_I) != 0);
	has_aux = (capability_mask & DB_ITEM_AUX_KW) != 0;
	
	if (has_power) {
		vm_get_range(VM_RANGE_POWER, &power_min, &power_max);
	}
	
	if (has_aux) {
		vm_get_range(VM_RANGE_AUX, &aux_min, &aux_max);
	}
}


static void _gui_tile_power_setup_power_meter()
{
	int32_t meter_min = (int32_t) power_min;
	int32_t meter_max = (int32_t) power_max;
	int32_t meter_pos_range;
	uint16_t meter_ticks = gui_utility_setup_large_270_meter_ticks(power_min, power_max);
	lv_meter_indicator_t * indic;
	
	// Meter background
    meter_power = lv_meter_create(tile);
    lv_obj_center(meter_power);
    lv_obj_set_size(meter_power, tile_w, tile_h);

    // Remove the circle from the middle
    lv_obj_remove_style(meter_power, NULL, LV_PART_INDICATOR);

    // Add a scale first
    lv_meter_scale_t * scale = lv_meter_add_scale(meter_power);
    lv_meter_set_scale_ticks(meter_power, scale, meter_ticks, 2, 20, lv_palette_main(LV_PALETTE_GREY));
    lv_meter_set_scale_major_ticks(meter_power, scale, 2, 3, 30, lv_color_hex3(0xeee), 20);
    lv_meter_set_scale_range(meter_power, scale, meter_min, meter_max, 270, 135);
    lv_obj_set_style_text_font(meter_power, &lv_font_montserrat_18, LV_PART_MAIN);
    
    // Add a blue arc for the negative (regen) part of the meter
	indic = lv_meter_add_arc(meter_power, scale, 5, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_meter_set_indicator_start_value(meter_power, indic, meter_min);
    lv_meter_set_indicator_end_value(meter_power, indic, 0);
    
    // Make the tick lines blue for the negative (regen) part of the meter
    indic = lv_meter_add_scale_lines(meter_power, scale, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_BLUE), false, 0);
    lv_meter_set_indicator_start_value(meter_power, indic, meter_min);
    lv_meter_set_indicator_end_value(meter_power, indic, 0);
    
    // Add a green arc for the first 20% of the postive part of the meter ("ECO" range)
    meter_pos_range = (int32_t)(power_max * 0.2);
    indic = lv_meter_add_arc(meter_power, scale, 5, lv_palette_main(LV_PALETTE_GREEN), 0);
	lv_meter_set_indicator_start_value(meter_power, indic, 0);
    lv_meter_set_indicator_end_value(meter_power, indic, meter_pos_range);
    
    // Make tick lines green for "ECO" range
    indic = lv_meter_add_scale_lines(meter_power, scale, lv_palette_main(LV_PALETTE_GREEN), lv_palette_main(LV_PALETTE_GREEN), false, 0);
    lv_meter_set_indicator_start_value(meter_power, indic, 0);
    lv_meter_set_indicator_end_value(meter_power, indic, meter_pos_range);
    
    // Add a red arc for the last 20% of the positive part of the meter
    indic = lv_meter_add_arc(meter_power, scale, 5, lv_palette_main(LV_PALETTE_RED), 0);
    lv_meter_set_indicator_start_value(meter_power, indic, meter_max - meter_pos_range);
    lv_meter_set_indicator_end_value(meter_power, indic, meter_max);

	// Make tick lines red
    indic = lv_meter_add_scale_lines(meter_power, scale, lv_palette_main(LV_PALETTE_RED), lv_palette_main(LV_PALETTE_RED), false, 0);
    lv_meter_set_indicator_start_value(meter_power, indic, meter_max - meter_pos_range);
    lv_meter_set_indicator_end_value(meter_power, indic, meter_max);


	// Create a green arc that will act as the positive meter indicator (traction power)
	power_pos_arc = lv_arc_create(tile);
	lv_obj_center(power_pos_arc);
	lv_obj_set_size(power_pos_arc, tile_w-10, tile_h-10);
	lv_arc_set_rotation(power_pos_arc, 135 + (270 * (-meter_min) / (meter_max - meter_min)));
	lv_arc_set_bg_angles(power_pos_arc, 0, 270 - (270 * (-meter_min) / (meter_max - meter_min)));
	lv_arc_set_range(power_pos_arc, 0, meter_max);
	lv_arc_set_value(power_pos_arc, 0);
	lv_obj_set_style_bg_color(power_pos_arc, lv_palette_main(LV_PALETTE_BLUE_GREY), LV_PART_INDICATOR);
	lv_obj_remove_style(power_pos_arc, NULL, LV_PART_KNOB);
	lv_obj_clear_flag(power_pos_arc, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_set_style_arc_color(power_pos_arc, lv_palette_main(LV_PALETTE_GREEN), LV_PART_INDICATOR);
	
	// Create a blue arc that will act as the negative meter indicator (regen power)
	power_neg_arc = lv_arc_create(tile);
	lv_obj_center(power_neg_arc);
	lv_obj_set_size(power_neg_arc, tile_w-10, tile_h-10);
	lv_arc_set_rotation(power_neg_arc, 135);
	lv_arc_set_bg_angles(power_neg_arc, 0, 270 * (-meter_min) / (meter_max - meter_min));
	lv_arc_set_range(power_neg_arc, 0, -meter_min);
	lv_arc_set_value(power_neg_arc, 0);
	lv_obj_set_style_bg_color(power_neg_arc, lv_palette_main(LV_PALETTE_BLUE_GREY), LV_PART_INDICATOR);
	lv_obj_remove_style(power_neg_arc, NULL, LV_PART_KNOB);
	lv_obj_clear_flag(power_neg_arc, LV_OBJ_FLAG_CLICKABLE);
	lv_arc_set_mode(power_neg_arc, LV_ARC_MODE_REVERSE);
	
	// Add the label object for the current power value
	power_val_lbl = lv_label_create(tile);
	lv_label_set_long_mode(power_val_lbl, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_align(power_val_lbl, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_set_style_text_font(power_val_lbl, &lv_font_montserrat_48, LV_PART_MAIN);
	lv_obj_align(power_val_lbl, LV_ALIGN_CENTER, 0, -40);
	
	// Initialize the meter to 0
	power_kw = -1.0;  // Force update
	_gui_tile_power_update_power_meter(0, true);
}


static void _gui_tile_power_setup_aux_meter()
{
	uint16_t w = 6*tile_w/16;
	uint16_t h = 6*tile_h/16;
	int32_t meter_min = (int32_t) aux_min;
	int32_t meter_max = (int32_t) aux_max;
	uint16_t meter_ticks = gui_utility_setup_small_270_meter_ticks(aux_min, aux_max);
	
	// Meter
	meter_aux = lv_meter_create(tile);
    lv_obj_align(meter_aux, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_size(meter_aux, w, h);
    lv_obj_remove_style(meter_aux, NULL, LV_PART_INDICATOR);
    
    // Meter scale (kW)
    lv_meter_scale_t* scale = lv_meter_add_scale(meter_aux);
    lv_obj_set_style_border_color(meter_aux, lv_palette_main(LV_PALETTE_BLUE_GREY), LV_PART_MAIN);
    lv_meter_set_scale_ticks(meter_aux, scale, meter_ticks, 3, 6, lv_palette_main(LV_PALETTE_GREY));
    lv_meter_set_scale_major_ticks(meter_aux, scale, 2, 3, 10, lv_color_hex3(0xeee), 10);
    lv_meter_set_scale_range(meter_aux, scale, meter_min, meter_max, 270, 135);

    // Green Meter arc (kW * 10)
    aux_arc = lv_arc_create(tile);
    lv_obj_align(aux_arc, LV_ALIGN_BOTTOM_MID, 0, -25);
	lv_obj_set_size(aux_arc, w-10, h-10);
    lv_arc_set_rotation(aux_arc, 135);
	lv_arc_set_bg_angles(aux_arc, 0, 270);
	lv_arc_set_range(aux_arc, meter_min * 10, meter_max * 10);
	lv_obj_set_style_bg_color(aux_arc, lv_palette_main(LV_PALETTE_BLUE_GREY), LV_PART_INDICATOR);
	lv_obj_remove_style(aux_arc, NULL, LV_PART_KNOB);
	lv_obj_clear_flag(aux_arc, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_set_style_arc_color(aux_arc, lv_palette_main(LV_PALETTE_GREEN), LV_PART_INDICATOR);
	
	// Value label
	aux_val_lbl = lv_label_create(tile);
	lv_label_set_long_mode(aux_val_lbl, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_align(aux_val_lbl, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_set_style_text_font(aux_val_lbl, &lv_font_montserrat_24, LV_PART_MAIN);
	lv_obj_align(aux_val_lbl, LV_ALIGN_BOTTOM_MID, 0, -h/2 - 10);
    
    // AUX label
    aux_lbl = lv_label_create(tile);
	lv_label_set_long_mode(aux_lbl, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_align(aux_lbl, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_set_style_text_font(aux_lbl, &lv_font_montserrat_18, LV_PART_MAIN);
	lv_obj_align(aux_lbl, LV_ALIGN_BOTTOM_MID, 0, -30);
	lv_label_set_text_static(aux_lbl, "AUX");
	
	// Initialize meter to 0
	_gui_tile_power_update_aux_meter(0);
}


static void _gui_tile_power_update_power_meter(int32_t val, bool immediate)
{
	static char kw_lbl[16];                  // "-XXX kW"
	uint32_t anim_time;
	
	// Update the label immediately
	sprintf(kw_lbl, "%ld kW", val);
	lv_label_set_text_static(power_val_lbl, kw_lbl);
	
	if (immediate) {
		_gui_tile_power_set_power_meter_cb(NULL, val);
	} else {
		// Stop any previous animations
		lv_anim_del(meter_power, _gui_tile_power_set_power_meter_cb);
		
		// Start an animation to the new value for the meter indicator
		anim_time = gui_utility_get_update_period() - 20;   // Just slightly faster than the average update interval
		lv_anim_init(&power_animation);
		lv_anim_set_exec_cb(&power_animation, _gui_tile_power_set_power_meter_cb);
		lv_anim_set_var(&power_animation, meter_power); // Unused since we will update the arcs
		lv_anim_set_time(&power_animation, anim_time);
		lv_anim_set_values(&power_animation, power_kw, val);
		lv_anim_start(&power_animation);
	}
}


static void _gui_tile_power_set_power_meter_cb(void* indic, int32_t val)
{
	if (val < 0) {
		lv_arc_set_value(power_pos_arc, 0);
		lv_arc_set_value(power_neg_arc, ((int32_t) round(-power_min)) + val);
	} else {
		lv_arc_set_value(power_neg_arc, (int32_t) round(-power_min));
		lv_arc_set_value(power_pos_arc, val);
	}
}


static void _gui_tile_power_update_aux_meter(float val)
{
	static char kw_lbl[12];                 // "XX.X"
	int32_t arc_val;
	
	arc_val = (int32_t) round(val * 10.0);
	lv_arc_set_value(aux_arc, arc_val);
	
	sprintf(kw_lbl, "%1.1f", val);
	lv_label_set_text_static(aux_val_lbl, kw_lbl);
}


static void _gui_tile_power_hv_v_cb(float val)
{
	hv_v = val;
	// Power updated when we get the current value
}


static void _gui_tile_power_hv_i_cb(float val)
{
	int32_t p;
	
	// Use this update to mark the intervals for the update timer
	gui_utility_note_update();
	
	// Update meter on receiving current (2nd after voltage)
	//  Negate current since we want to display positive kW for traction
	//  and negative kW for regeneration (battery current is negative for traction)
	val = -val;
	p = round((hv_v * val) / 1000.0);
	if (p != power_kw) {
		_gui_tile_power_update_power_meter(p, false);
		power_kw = p;
	}
}


static void _gui_tile_power_aux_cb(float val)
{
	if (val != aux_kw) {
		_gui_tile_power_update_aux_meter(val);
		aux_kw = val;
	}
}
