/*
 * Wifi settings screen.
 *
 * Displays Wifi OBD Adapter settings: SSID, PW, Port.  Opens keyboard popup when
 * various settings are touched to allow changes.  Provides a Save or Cancel button
 * to return to Settings tile.
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
#include "esp_log.h"
#include "gui_screen_wifi.h"
#include "gui_task.h"
#include "ps_utilities.h"
#include <stdlib.h>
#include <string.h>



//
// Private constants
//

// Maximum length of port string
#define MAX_PORT_DIGITS   5

// Indicies for keyboard to associate value with
#define VAL_SSID_INDEX    0
#define VAL_PW_INDEX      1
#define VAL_PORT_INDEX    2



//
// Variables
//
static const char* TAG = "gui_screen_wifi";

// LVGL Objects
static lv_obj_t* page;

static lv_obj_t* title_lbl;

static lv_obj_t* ssid;
static lv_obj_t* ssid_lbl;

static lv_obj_t* pw;
static lv_obj_t* pw_lbl;

static lv_obj_t* port;
static lv_obj_t* port_lbl;

static lv_obj_t* cancel_btn;
static lv_obj_t* cancel_btn_lbl;
static lv_obj_t* save_btn;
static lv_obj_t* save_btn_lbl;

// State
static uint16_t screen_w;
static uint16_t screen_h;
static uint16_t vertical_spacing;
static uint16_t row_y;

static net_config_t* configP;
static char cur_ssid[PS_SSID_MAX_LEN+1];
static char cur_pw[PS_PW_MAX_LEN+1];
static char cur_port[MAX_PORT_DIGITS+1];



//
// Forward declarations for internal functions
//
static void _gui_screen_wifi_setup_ssid();
static void _gui_screen_wifi_setup_pw();
static void _gui_screen_wifi_setup_port();
static void _gui_screen_wifi_setup_buttons();
static void _gui_screen_wifi_val_cb(lv_event_t* e);
static void _gui_screen_wifi_btn_cb(lv_event_t* e);
static void _gui_screen_wifi_update_textfield(int index, char* val);  // For pop-up keyboard



//
// API
//
lv_obj_t* gui_screen_wifi_init()
{
	gui_get_screen_size(&screen_w, &screen_h);
	
	// Create the top-level page object
	page = lv_obj_create(NULL);
	lv_obj_set_pos(page, 0, 0);
	lv_obj_set_size(page, screen_w, screen_h);
	vertical_spacing = screen_h / 7;            // Matches spacing of settings tile
	row_y = vertical_spacing;
	
	// Title our page
	title_lbl = lv_label_create(page);
	lv_label_set_long_mode(title_lbl, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_align(title_lbl, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_30, LV_PART_MAIN);
	lv_obj_set_width(title_lbl, screen_w);
	lv_obj_set_pos(title_lbl, 0, row_y);
	lv_label_set_text_static(title_lbl, "Wifi");
	row_y += vertical_spacing;
	
	// Add the controls
	_gui_screen_wifi_setup_ssid();
	_gui_screen_wifi_setup_pw();
	_gui_screen_wifi_setup_port();
	_gui_screen_wifi_setup_buttons();
	
	// Get a pointer to the persistent storage network configuration
	(void) ps_get_config(PS_CONFIG_TYPE_NET, (void**) &configP);
	
	return page;
}


void gui_screen_wifi_set_active(bool is_active)
{
	if (is_active) {
		// Set our values based on persistent storage and display them
		strncpy(cur_ssid, configP->sta_ssid, PS_SSID_MAX_LEN);
		lv_label_set_text_static(ssid, cur_ssid);
		
		strncpy(cur_pw, configP->sta_pw, PS_PW_MAX_LEN);
		lv_label_set_text_static(pw, cur_pw);
		
		sprintf(cur_port, "%u", configP->remote_port);
		lv_label_set_text_static(port, cur_port);
	}
}



//
// Internal functions
//
static void _gui_screen_wifi_setup_ssid()
{
	// Create the control label
	ssid_lbl = lv_label_create(page);
	lv_label_set_long_mode(ssid_lbl, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_align(ssid_lbl, LV_TEXT_ALIGN_RIGHT, 0);
	lv_obj_set_style_text_font(ssid_lbl, &lv_font_montserrat_24, LV_PART_MAIN);
	lv_obj_set_pos(ssid_lbl, 0, row_y);
	lv_obj_set_width(ssid_lbl, screen_w/2 - 5);
	lv_label_set_text_static(ssid_lbl, "SSID");
	lv_obj_add_flag(ssid_lbl, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_add_event_cb(ssid_lbl, _gui_screen_wifi_val_cb, LV_EVENT_CLICKED, NULL);
	
	// Create the SSID value label
	ssid = lv_label_create(page);
	lv_label_set_long_mode(ssid, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_align(ssid, LV_TEXT_ALIGN_LEFT, 0);
	lv_obj_set_style_text_color(ssid, lv_palette_main(LV_PALETTE_LIGHT_BLUE), LV_PART_MAIN);
	lv_obj_set_style_text_font(ssid, &lv_font_montserrat_24, LV_PART_MAIN);
	lv_obj_set_pos(ssid, screen_w/2 + 5, row_y);
	lv_obj_set_width(ssid, screen_w/2 - 5);
	lv_obj_add_flag(ssid, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_add_event_cb(ssid, _gui_screen_wifi_val_cb, LV_EVENT_CLICKED, NULL);
	
	row_y += vertical_spacing;
}


static void _gui_screen_wifi_setup_pw()
{
	// Create the control label
	pw_lbl = lv_label_create(page);
	lv_label_set_long_mode(pw_lbl, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_align(pw_lbl, LV_TEXT_ALIGN_RIGHT, 0);
	lv_obj_set_style_text_font(pw_lbl, &lv_font_montserrat_24, LV_PART_MAIN);
	lv_obj_set_pos(pw_lbl, 0, row_y);
	lv_obj_set_width(pw_lbl, screen_w/2 - 5);
	lv_label_set_text_static(pw_lbl, "Password");
	lv_obj_add_flag(pw_lbl, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_add_event_cb(pw_lbl, _gui_screen_wifi_val_cb, LV_EVENT_CLICKED, NULL);
	
	// Create the SSID value label
	pw = lv_label_create(page);
	lv_label_set_long_mode(pw, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_align(pw, LV_TEXT_ALIGN_LEFT, 0);
	lv_obj_set_style_text_color(pw, lv_palette_main(LV_PALETTE_LIGHT_BLUE), LV_PART_MAIN);
	lv_obj_set_style_text_font(pw, &lv_font_montserrat_24, LV_PART_MAIN);
	lv_obj_set_pos(pw, screen_w/2 + 5, row_y);
	lv_obj_set_width(pw, screen_w/2 - 5);
	lv_obj_add_flag(pw, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_add_event_cb(pw, _gui_screen_wifi_val_cb, LV_EVENT_CLICKED, NULL);
	
	row_y += vertical_spacing;
}


static void _gui_screen_wifi_setup_port()
{
	// Create the control label
	port_lbl = lv_label_create(page);
	lv_label_set_long_mode(port_lbl, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_align(port_lbl, LV_TEXT_ALIGN_RIGHT, 0);
	lv_obj_set_style_text_font(port_lbl, &lv_font_montserrat_24, LV_PART_MAIN);
	lv_obj_set_pos(port_lbl, 0, row_y);
	lv_obj_set_width(port_lbl, screen_w/2 - 5);
	lv_label_set_text_static(port_lbl, "Port");
	lv_obj_add_flag(port_lbl, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_add_event_cb(port_lbl, _gui_screen_wifi_val_cb, LV_EVENT_CLICKED, NULL);
	
	// Create the SSID value label
	port = lv_label_create(page);
	lv_label_set_long_mode(port, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_align(port, LV_TEXT_ALIGN_LEFT, 0);
	lv_obj_set_style_text_color(port, lv_palette_main(LV_PALETTE_LIGHT_BLUE), LV_PART_MAIN);
	lv_obj_set_style_text_font(port, &lv_font_montserrat_24, LV_PART_MAIN);
	lv_obj_set_pos(port, screen_w/2 + 5, row_y);
	lv_obj_set_width(port, screen_w/2 - 5);
	lv_obj_add_flag(port, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_add_event_cb(port, _gui_screen_wifi_val_cb, LV_EVENT_CLICKED, NULL);
	
	row_y += vertical_spacing;
}


static void _gui_screen_wifi_setup_buttons()
{
	uint16_t btn_w = screen_w / 4;
	uint16_t btn_h = screen_h / 10;
	
	// Cancel button and label
	cancel_btn = lv_btn_create(page);
	lv_obj_set_size(cancel_btn, btn_w, btn_h);
	lv_obj_set_pos(cancel_btn, screen_w/2 - btn_w - btn_w/3, row_y);
	lv_obj_add_event_cb(cancel_btn, _gui_screen_wifi_btn_cb, LV_EVENT_ALL, NULL);
	
	cancel_btn_lbl = lv_label_create(cancel_btn);
	lv_obj_set_style_text_font(cancel_btn_lbl, &lv_font_montserrat_30, LV_PART_MAIN);
	lv_label_set_text_static(cancel_btn_lbl, "Cancel");
	lv_obj_center(cancel_btn_lbl);
	
	// Save button and label
	save_btn = lv_btn_create(page);
	lv_obj_set_size(save_btn, btn_w, btn_h);
	lv_obj_set_pos(save_btn, screen_w/2 + btn_w/3, row_y);
	lv_obj_add_event_cb(save_btn, _gui_screen_wifi_btn_cb, LV_EVENT_ALL, NULL);
	
	save_btn_lbl = lv_label_create(save_btn);
	lv_obj_set_style_text_font(save_btn_lbl, &lv_font_montserrat_30, LV_PART_MAIN);
	lv_label_set_text_static(save_btn_lbl, "Save");
	lv_obj_center(save_btn_lbl);
	
	row_y += vertical_spacing;
}


static void _gui_screen_wifi_val_cb(lv_event_t* e)
{
	lv_event_code_t code = lv_event_get_code(e);
	lv_obj_t* obj = lv_event_get_target(e);
	
	if (code == LV_EVENT_CLICKED) {
		// Display a keyboard with the selected value to edit
		if ((obj == ssid_lbl) || (obj == ssid)) {
			gui_utility_display_alpha_kbd(page, "SSID", VAL_SSID_INDEX, cur_ssid, PS_SSID_MAX_LEN, &_gui_screen_wifi_update_textfield);
		} else if ((obj == pw_lbl) || (obj == pw)) {
			gui_utility_display_alpha_kbd(page, "Password", VAL_PW_INDEX, cur_pw, PS_PW_MAX_LEN, &_gui_screen_wifi_update_textfield);
		} else if ((obj == port_lbl) || (obj == port)) {
			gui_utility_display_numeric_kbd(page, "Port", VAL_PORT_INDEX, cur_port, MAX_PORT_DIGITS, &_gui_screen_wifi_update_textfield);
		}
	}
}


static void _gui_screen_wifi_btn_cb(lv_event_t* e)
{
	bool changed = false;
	uint16_t port_num;
	
	lv_event_code_t code = lv_event_get_code(e);
	lv_obj_t* obj = lv_event_get_target(e);
	
	if (code == LV_EVENT_CLICKED) {
		if (obj == cancel_btn) {
			// Return to settings display with no changes
			gui_set_screen_page(GUI_SCREEN_MAIN);
		} else if (obj == save_btn) {
			// Determine if there are any changes
			if (strcmp(configP->sta_ssid, cur_ssid) != 0) {
				strcpy(configP->sta_ssid, cur_ssid);
				changed = true;
			}
			
			if (strcmp(configP->sta_pw, cur_pw) != 0) {
				strcpy(configP->sta_pw, cur_pw);
				changed = true;
			}
			
			port_num = (uint16_t) atoi(cur_port);
			if (configP->remote_port != port_num) {
				configP->remote_port = port_num;
				changed = true;
			}
			
			// Save to persistent storage if necessary
			if (changed) {
				if (ps_save_config(PS_CONFIG_TYPE_NET)) {
					ESP_LOGI(TAG, "Updated persistent storage");
				} else {
					ESP_LOGE(TAG, "Could not update persistent storage");
				}
			} else {
				ESP_LOGI(TAG, "No changes detected on Save press");
			}
			
			// Return to settings display
			gui_set_screen_page(GUI_SCREEN_MAIN);
		}
	}
}


static void _gui_screen_wifi_update_textfield(int index, char* val)
{
	switch (index) {
		case VAL_SSID_INDEX:
			strcpy(cur_ssid, val);
			lv_label_set_text_static(ssid, cur_ssid);
			break;
		case VAL_PW_INDEX:
			strcpy(cur_pw, val);
			lv_label_set_text_static(pw, cur_pw);
			break;
		case VAL_PORT_INDEX:
			strcpy(cur_port, val);
			lv_label_set_text_static(port, cur_port);
			break;
	}
}
