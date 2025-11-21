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
#ifndef GUI_UTILITIES_H
#define GUI_UTILITIES_H

#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>


//
// Popup Keyboard update function
//
typedef void (*gui_utility_kbd_update_textfield)(int index, char* val);



//
// API
//
float gui_util_c_to_f(float c);
float gui_util_m_to_feet(float m);
float gui_util_kph_to_mph(float kph);

// Return number of ticks to display various sized meter
uint16_t gui_utility_setup_large_270_meter_ticks(float min, float max);
uint16_t gui_utility_setup_small_180_meter_ticks(float min, float max);
uint16_t gui_utility_setup_small_270_meter_ticks(float min, float max);

// Functions used to detect average interval between updates for meter animation purposes
void gui_utility_init_update_time(uint32_t init_delay);
void gui_utility_note_update();
uint32_t gui_utility_get_update_period();

// Pop-up keyboards for configuration updating
void gui_utility_display_alpha_kbd(lv_obj_t* parent, char* title, int index, char* val, int val_len, gui_utility_kbd_update_textfield cb_fcn);
void gui_utility_display_numeric_kbd(lv_obj_t* parent, char* title, int index, char* val, int val_len, gui_utility_kbd_update_textfield cb_fcn);
void gui_utility_display_hex_kbd(lv_obj_t* parent, char* title, int index, char* val, int val_len, gui_utility_kbd_update_textfield cb_fcn);

void gui_dump_mem_info();

#endif /* GUI_UTILITIES_H */
