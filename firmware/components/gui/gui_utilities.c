/*
 * Utility functions
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
#include "esp_timer.h"
#include "esp_log.h"
#include "gui_utilities.h"



//
// Private constants
//
#define NUM_UPDATE_PERIODS          2

// Keypad pop-up related
//
// Keypad pop-up types
#define GUI_KEYPAD_TYPE_ALPHA       0
#define GUI_KEYPAD_TYPE_NUMERIC     1
#define GUI_KEYPAD_TYPE_HEX         2


//
// Variables
//
static const char* TAG = "gui_utilities";

static bool wait_first_timestamp;
static int timestamp_delta_index;
static int64_t prev_timestamp;
static int32_t timestamp_deltas[NUM_UPDATE_PERIODS];

// Keypad pop-up
static lv_obj_t* kp_popup = NULL;
static lv_obj_t* kp_title_lbl;
static lv_obj_t* kp_value_ta;
static lv_obj_t* kp_btnm;

static const char* numeric_map[] =
	{"1", "2", "3", LV_SYMBOL_LEFT, "\n",
	 "4", "5", "6", LV_SYMBOL_RIGHT, "\n",
	 "7", "8", "9", LV_SYMBOL_BACKSPACE, "\n",
	 LV_SYMBOL_CLOSE, "0", LV_SYMBOL_OK, ""
	};
static const lv_btnmatrix_ctrl_t numeric_ctrl_map[] =
	{LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT,
	 LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT,
	 LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT,
	 LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT
	};

static const char* hex_map[] = 
	{"0", "1", "2", "3", "\n",
	 "4", "5", "6", "7", "\n",
	 "8", "9", "a", "b", "\n",
	 "c", "d", "e", "f", "\n",
	 LV_SYMBOL_LEFT, LV_SYMBOL_BACKSPACE, LV_SYMBOL_RIGHT, "\n",
	 LV_SYMBOL_CLOSE, LV_SYMBOL_OK, ""
	};
static const lv_btnmatrix_ctrl_t hex_ctrl_map[] = 
	{LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT,
	 LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT,
	 LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT,
	 LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT,
	 LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT,
	 LV_BTNMATRIX_CTRL_NO_REPEAT, LV_BTNMATRIX_CTRL_NO_REPEAT
	};
	
static int kp_orig_control_index;
static int kp_type;
static gui_utility_kbd_update_textfield kp_fcn_parent_val_cb;



//
// Forward declarations for internal functions
//
uint16_t _gui_util_setup_meter_ticks(float major_tick_value, float major_tick_inc, int min_ticks, int max_ticks, float min_val, float max_val);
float _gui_util_div_frac(float dividend, float divisor);
void _gui_util_display_keypad(lv_obj_t* parent, char* title, char* val, int val_len);
void _gui_util_keypad_cb(lv_event_t* e);



//
// API
//
float gui_util_c_to_f(float c)
{
	return ((9 * c / 5) + 32);
}


float gui_util_m_to_feet(float m)
{
	return (m * 3.28084);
}


float gui_util_kph_to_mph(float kph)
{
	return (kph / 1.60934);
}


uint16_t gui_utility_setup_large_270_meter_ticks(float min, float max)
{
	return _gui_util_setup_meter_ticks(20.0, 10.0, 11, 25, min, max);
}


uint16_t gui_utility_setup_small_180_meter_ticks(float min, float max)
{
	return _gui_util_setup_meter_ticks(1.0, 1.0, 5, 9, min, max);
}


uint16_t gui_utility_setup_small_270_meter_ticks(float min, float max)
{
	return _gui_util_setup_meter_ticks(1.0, 1.0, 11, 15, min, max);
}


void gui_utility_init_update_time(uint32_t init_delay)
{
	// Initialize our delta array with this value
	for (int i=0; i<NUM_UPDATE_PERIODS; i++) {
		timestamp_deltas[i] = init_delay;
	}
	
	// Setup to start collecting delta times between samples
	wait_first_timestamp = true;
	timestamp_delta_index = 0;
}


void gui_utility_note_update()
{
	int64_t cur_timestamp;
	
	cur_timestamp = esp_timer_get_time();  // uSec
	
	if (wait_first_timestamp) {
		// This update is the first so we just collect the time
		prev_timestamp = cur_timestamp;
		wait_first_timestamp = false;
	} else {
		// Add the current delta to our averaging array
		timestamp_deltas[timestamp_delta_index++] = (int32_t) ((cur_timestamp - prev_timestamp) / 1000);  // Convert to mSec
		if (timestamp_delta_index >= NUM_UPDATE_PERIODS) timestamp_delta_index = 0;
		prev_timestamp = cur_timestamp;
	}
}


uint32_t gui_utility_get_update_period()
{
	uint32_t sum = 0;
	
	// Compute an average
	for (int i=0; i<NUM_UPDATE_PERIODS; i++) {
		sum += (uint32_t) timestamp_deltas[i];
	}
	
	return (sum/NUM_UPDATE_PERIODS);
}


void gui_utility_display_alpha_kbd(lv_obj_t* parent, char* title, int index, char* val, int val_len, gui_utility_kbd_update_textfield cb_fcn)
{
	if (kp_popup == NULL) {
		kp_orig_control_index = index;
		kp_type = GUI_KEYPAD_TYPE_ALPHA;
		kp_fcn_parent_val_cb = cb_fcn;
		
		_gui_util_display_keypad(parent, title, val, val_len);
	}
}


void gui_utility_display_numeric_kbd(lv_obj_t* parent, char* title, int index, char* val, int val_len, gui_utility_kbd_update_textfield cb_fcn)
{
	if (kp_popup == NULL) {
		kp_orig_control_index = index;
		kp_type = GUI_KEYPAD_TYPE_NUMERIC;
		kp_fcn_parent_val_cb = cb_fcn;
		
		_gui_util_display_keypad(parent, title, val, val_len);
	}
}


void gui_utility_display_hex_kbd(lv_obj_t* parent, char* title, int index, char* val, int val_len, gui_utility_kbd_update_textfield cb_fcn)
{
	if (kp_popup == NULL) {
		kp_orig_control_index = index;
		kp_type = GUI_KEYPAD_TYPE_HEX;
		kp_fcn_parent_val_cb = cb_fcn;
		
		_gui_util_display_keypad(parent, title, val, val_len);
	}
}

void gui_dump_mem_info()
{

	lv_mem_monitor_t mem_info;
	
	// Get LVGL's current private heap info
	lv_mem_monitor(&mem_info);
	
	ESP_LOGI(TAG, "LVGL Memory Statistics:");
	ESP_LOGI(TAG, "  Total size: %lu", mem_info.total_size);
	ESP_LOGI(TAG, "  Free Count: %lu   Free Size: %lu   Free Biggest Size: %lu", mem_info.free_cnt, mem_info.free_size, mem_info.free_biggest_size);
	ESP_LOGI(TAG, "  Used Count: %lu   Max Used: %lu  Used Percent: %u", mem_info.used_cnt, mem_info.max_used, mem_info.used_pct);
	ESP_LOGI(TAG, "  Frag Percent: %u", mem_info.frag_pct);
}



//
// Internal functions
//

//
// major_tick_value - Starting count between major (alternating) tick values
// major_tick_inc - Increment to add to major_tick_value if it results in too many ticks (more than max_ticks)
// min_ticks, max_ticks - Minimum and maximum number of ticks to generate (major + minor ticks)
// min_val, max_val - Range of meter
uint16_t _gui_util_setup_meter_ticks(float major_tick_value, float major_tick_inc, int min_ticks, int max_ticks, float min_val, float max_val)
{
	bool done = false;
	float range_delta = max_val - min_val;
	int calc_ticks;
	
	// Find the first major tick increment that fits (we draw labeled major tick marks and an intermediary,
	// unlabled minor tick)
	while (!done) {
		// Calculate the tick count for the current major_tick_val
		calc_ticks = 2 * (int)(range_delta / major_tick_value) + 1;
		
		if ((_gui_util_div_frac(range_delta, major_tick_value) != 0) && (calc_ticks > min_ticks)) {
			// This major tick value does not fit evenly into the range so try the next major_tick_val
			major_tick_value += major_tick_inc;
			continue;
		}
		
		if (calc_ticks > max_ticks) {
			// Too many ticks so try the next major_tick_val
			major_tick_value += major_tick_inc;
			continue;
		}
		
		// We found something reasonable
		done = true;
	}
	
	return (uint16_t) calc_ticks;
}


float _gui_util_div_frac(float dividend, float divisor)
{
	float f;
	int n;
	
	f = dividend / divisor;
	n = dividend / divisor;
	
	return (f - (float) n);
}


void _gui_util_display_keypad(lv_obj_t* parent, char* title, char* val, int val_len)
{
	uint16_t parent_w = lv_obj_get_width(parent);
	uint16_t parent_h = lv_obj_get_height(parent);
	uint16_t kbd_h;
	uint16_t kbd_h_offset;
	
	// Keypad height depends slightly on what kind it is
	if (kp_type == GUI_KEYPAD_TYPE_HEX) {
		kbd_h = parent_h/2;
		kbd_h_offset = 40;
	} else {
		kbd_h = parent_h/3;
		kbd_h_offset = 0;
	}
	
	// Create the keypad object
	kp_popup = lv_obj_create(parent);
	lv_obj_set_pos(kp_popup, 0, 0);
	lv_obj_set_size(kp_popup, parent_w, parent_h);
	lv_obj_set_style_pad_left(kp_popup, 0, LV_STATE_DEFAULT);
	lv_obj_set_style_pad_right(kp_popup, 0, LV_STATE_DEFAULT);
	lv_obj_set_scrollbar_mode(kp_popup, LV_SCROLLBAR_MODE_OFF);
	
	// Create the keypad title label
	kp_title_lbl = lv_label_create(kp_popup);
	lv_label_set_long_mode(kp_title_lbl, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_align(kp_title_lbl, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_set_style_text_font(kp_title_lbl, &lv_font_montserrat_24, LV_PART_MAIN);
	lv_obj_set_width(kp_title_lbl, parent_w);
	lv_obj_align(kp_title_lbl, LV_ALIGN_CENTER, 0, -(kbd_h/2 + 90) + kbd_h_offset);
	lv_label_set_text(kp_title_lbl, title);
	
	// Create the local text display of entered value
	kp_value_ta = lv_textarea_create(kp_popup);
	lv_textarea_set_one_line(kp_value_ta, true);
	lv_textarea_set_max_length(kp_value_ta, val_len);
	lv_textarea_set_text(kp_value_ta, val);
	lv_obj_add_state(kp_value_ta, LV_STATE_FOCUSED);
	lv_obj_align(kp_value_ta, LV_ALIGN_CENTER, 0, -(kbd_h/2 + 40) + kbd_h_offset);
	
	// Create the button array
	kp_btnm = lv_keyboard_create(kp_popup);
	lv_btnmatrix_set_btn_ctrl_all(kp_btnm, LV_BTNMATRIX_CTRL_NO_REPEAT);
	if (kp_type == GUI_KEYPAD_TYPE_NUMERIC) {
		// Replace the default alphanumeric keymap with our custom numeric keymap
		lv_keyboard_set_map(kp_btnm, LV_KEYBOARD_MODE_USER_1, numeric_map, numeric_ctrl_map);
		lv_keyboard_set_mode(kp_btnm, LV_KEYBOARD_MODE_USER_1);
		lv_obj_set_size(kp_btnm, parent_w/2, kbd_h);
	} else if (kp_type == GUI_KEYPAD_TYPE_HEX) {
		// Replace the default alphanumeric keymap with our custom hexadecimal keymap
		lv_keyboard_set_map(kp_btnm, LV_KEYBOARD_MODE_USER_1, hex_map, hex_ctrl_map);
		lv_keyboard_set_mode(kp_btnm, LV_KEYBOARD_MODE_USER_1);
		lv_obj_set_size(kp_btnm, parent_w/2, kbd_h);
	} else {
		lv_obj_set_size(kp_btnm, parent_w - 40, kbd_h);
	}
	lv_obj_align(kp_btnm, LV_ALIGN_CENTER, 0, kbd_h_offset);
	lv_obj_add_event_cb(kp_btnm, _gui_util_keypad_cb, LV_EVENT_CLICKED, kp_value_ta);
	lv_obj_clear_flag(kp_btnm, LV_OBJ_FLAG_CLICK_FOCUSABLE);
	lv_keyboard_set_textarea(kp_btnm, kp_value_ta);
}


void _gui_util_keypad_cb(lv_event_t* e)
{
	lv_event_code_t code = lv_event_get_code(e);
	lv_obj_t* obj = lv_event_get_target(e);
	
	if (code == LV_EVENT_CLICKED) {
		const char* txt = lv_btnmatrix_get_btn_text(obj, lv_btnmatrix_get_selected_btn(obj));
		
		if ((strcmp(txt, LV_SYMBOL_CLOSE) == 0) || (strcmp(txt, LV_SYMBOL_KEYBOARD) == 0)) {
			// Close the popup without saving any changed text
			lv_obj_del(kp_popup);
			kp_popup = NULL;
		} else if (strcmp(txt, LV_SYMBOL_OK) == 0) {
			// Send the textfield back to the calling control
			kp_fcn_parent_val_cb(kp_orig_control_index, (char*) lv_textarea_get_text(kp_value_ta));
			
			// Close the popup without saving any changed text
			lv_obj_del(kp_popup);
			kp_popup = NULL;
		}
	}
}
