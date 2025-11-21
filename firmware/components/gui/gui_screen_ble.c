/*
 * BLE settings screen.
 *
 * Displays BLE OBD Adapter settings: [Known] Device pulldown, and direct UUID
 * fields for Service UUID, TX and RX Characteristic UUID.  Opens keyboard popup when
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
#include "ble_utilities.h"
#include "esp_system.h"
#include "esp_log.h"
#include "gui_screen_ble.h"
#include "gui_task.h"
#include "ps_utilities.h"
#include <stdlib.h>
#include <string.h>



//
// Private constants
//

// Maximum length of port string
#define MAX_PORT_DIGITS    4

// Indicies for keyboard to associate value with
#define VAL_SERVICE_INDEX  0
#define VAL_TX_UUID_INDEX  1
#define VAL_RX_UUID_INDEX  2



//
// Variables
//
static const char* TAG = "gui_screen_ble";

// LVGL Objects
static lv_obj_t* page;

static lv_obj_t* title_lbl;

static lv_obj_t* enable_custom_sw;
static lv_obj_t* enable_custom_sw_lbl;

static lv_obj_t* service_uuid;
static lv_obj_t* service_uuid_lbl;

static lv_obj_t* tx_uuid;
static lv_obj_t* tx_uuid_lbl;

static lv_obj_t* rx_uuid;
static lv_obj_t* rx_uuid_lbl;

static lv_obj_t* cancel_btn;
static lv_obj_t* cancel_btn_lbl;
static lv_obj_t* save_btn;
static lv_obj_t* save_btn_lbl;

// State
static uint16_t screen_w;
static uint16_t screen_h;
static uint16_t vertical_spacing;
static uint16_t row_y;

static ble_config_t* configP;
static bool cur_custom_enable;
static char cur_service_uuid_str[PS_BLE_UUID_STR_LEN];
static char cur_tx_char_uuid_str[PS_BLE_UUID_STR_LEN];
static char cur_rx_char_uuid_str[PS_BLE_UUID_STR_LEN];



//
// Forward declarations for internal functions
//
static void _gui_screen_ble_setup_enable();
static void _gui_screen_ble_setup_service();
static void _gui_screen_ble_setup_tx();
static void _gui_screen_ble_setup_rx();
static void _gui_screen_ble_setup_buttons();
static void _gui_screen_update_uuid_strings();
static void _gui_screen_ble_enable_cb(lv_event_t* e);
static void _gui_screen_ble_val_cb(lv_event_t* e);
static void _gui_screen_ble_btn_cb(lv_event_t* e);
static void _gui_screen_ble_update_textfield(int index, char* val);  // For pop-up keyboard



//
// API
//
lv_obj_t* gui_screen_ble_init()
{
	gui_get_screen_size(&screen_w, &screen_h);
	
	// Create the top-level page object
	page = lv_obj_create(NULL);
	lv_obj_set_pos(page, 0, 0);
	lv_obj_set_size(page, screen_w, screen_h);
	vertical_spacing = screen_h / 8;            // Matches spacing of settings tile
	row_y = vertical_spacing;
	
	// Title our page
	title_lbl = lv_label_create(page);
	lv_label_set_long_mode(title_lbl, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_align(title_lbl, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_30, LV_PART_MAIN);
	lv_obj_set_width(title_lbl, screen_w);
	lv_obj_set_pos(title_lbl, 0, row_y);
	lv_label_set_text_static(title_lbl, "BLE");
	row_y += vertical_spacing;
	
	// Add the controls
	_gui_screen_ble_setup_enable();
	_gui_screen_ble_setup_service();
	_gui_screen_ble_setup_tx();
	_gui_screen_ble_setup_rx();
	_gui_screen_ble_setup_buttons();
	
	// Get a pointer to the persistent storage BLE configuration
	(void) ps_get_config(PS_CONFIG_TYPE_BLE, (void**) &configP);
	
	return page;
}


void gui_screen_ble_set_active(bool is_active)
{
	if (is_active) {
		// Set our values based on persistent storage and display them
		cur_custom_enable = configP->use_custom_uuid;
		if (cur_custom_enable) {
			lv_obj_add_state(enable_custom_sw, LV_STATE_CHECKED);
		} else {
			lv_obj_clear_state(enable_custom_sw, LV_STATE_CHECKED);
		}
		
		strcpy(cur_service_uuid_str, configP->service_uuid);
		strcpy(cur_tx_char_uuid_str, configP->tx_char_uuid);
		strcpy(cur_rx_char_uuid_str, configP->rx_char_uuid);
		_gui_screen_update_uuid_strings();
	}
}



//
// Internal functions
//
static void _gui_screen_ble_setup_enable()
{
	// Create the control label
	enable_custom_sw_lbl = lv_label_create(page);
	lv_label_set_long_mode(enable_custom_sw_lbl, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_align(enable_custom_sw_lbl, LV_TEXT_ALIGN_RIGHT, 0);
	lv_obj_set_style_text_font(enable_custom_sw_lbl, &lv_font_montserrat_24, LV_PART_MAIN);
	lv_obj_set_pos(enable_custom_sw_lbl, 0, row_y);
	lv_obj_set_width(enable_custom_sw_lbl, screen_w/2 - 5);
	lv_label_set_text_static(enable_custom_sw_lbl, "Enable Custom");
	
	// Create the enable custom uuids switch
	enable_custom_sw = lv_switch_create(page);
	lv_obj_set_width(enable_custom_sw, screen_w/6);
	lv_obj_set_pos(enable_custom_sw, screen_w/2 + 5, row_y);
	lv_obj_add_event_cb(enable_custom_sw, _gui_screen_ble_enable_cb, LV_EVENT_VALUE_CHANGED, NULL);
	
	row_y += vertical_spacing;
}


static void _gui_screen_ble_setup_service()
{
	// Create the control label
	service_uuid_lbl = lv_label_create(page);
	lv_label_set_long_mode(service_uuid_lbl, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_align(service_uuid_lbl, LV_TEXT_ALIGN_RIGHT, 0);
	lv_obj_set_style_text_font(service_uuid_lbl, &lv_font_montserrat_24, LV_PART_MAIN);
	lv_obj_set_pos(service_uuid_lbl, 0, row_y);
	lv_obj_set_width(service_uuid_lbl, screen_w/2 - 5);
	lv_label_set_text_static(service_uuid_lbl, "Service UUID");
	lv_obj_add_flag(service_uuid_lbl, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_add_event_cb(service_uuid_lbl, _gui_screen_ble_val_cb, LV_EVENT_CLICKED, NULL);
	
	// Create the SSID value label
	service_uuid = lv_label_create(page);
	lv_label_set_long_mode(service_uuid, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_align(service_uuid, LV_TEXT_ALIGN_LEFT, 0);
	lv_obj_set_style_text_color(service_uuid, lv_palette_main(LV_PALETTE_LIGHT_BLUE), LV_PART_MAIN);
	lv_obj_set_style_text_font(service_uuid, &lv_font_montserrat_24, LV_PART_MAIN);
	lv_obj_set_pos(service_uuid, screen_w/2 + 5, row_y);
	lv_obj_set_width(service_uuid, screen_w/2 - 5);
	lv_obj_add_flag(service_uuid, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_add_event_cb(service_uuid, _gui_screen_ble_val_cb, LV_EVENT_CLICKED, NULL);
	
	row_y += vertical_spacing;
}


static void _gui_screen_ble_setup_tx()
{
	// Create the control label
	tx_uuid_lbl = lv_label_create(page);
	lv_label_set_long_mode(tx_uuid_lbl, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_align(tx_uuid_lbl, LV_TEXT_ALIGN_RIGHT, 0);
	lv_obj_set_style_text_font(tx_uuid_lbl, &lv_font_montserrat_24, LV_PART_MAIN);
	lv_obj_set_pos(tx_uuid_lbl, 0, row_y);
	lv_obj_set_width(tx_uuid_lbl, screen_w/2 - 5);
	lv_label_set_text_static(tx_uuid_lbl, "TX Char UUID");
	lv_obj_add_flag(tx_uuid_lbl, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_add_event_cb(tx_uuid_lbl, _gui_screen_ble_val_cb, LV_EVENT_CLICKED, NULL);
	
	// Create the SSID value label
	tx_uuid = lv_label_create(page);
	lv_label_set_long_mode(tx_uuid, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_align(tx_uuid, LV_TEXT_ALIGN_LEFT, 0);
	lv_obj_set_style_text_color(tx_uuid, lv_palette_main(LV_PALETTE_LIGHT_BLUE), LV_PART_MAIN);
	lv_obj_set_style_text_font(tx_uuid, &lv_font_montserrat_24, LV_PART_MAIN);
	lv_obj_set_pos(tx_uuid, screen_w/2 + 5, row_y);
	lv_obj_set_width(tx_uuid, screen_w/2 - 5);
	lv_obj_add_flag(tx_uuid, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_add_event_cb(tx_uuid, _gui_screen_ble_val_cb, LV_EVENT_CLICKED, NULL);
	
	row_y += vertical_spacing;
}


static void _gui_screen_ble_setup_rx()
{
	// Create the control label
	rx_uuid_lbl = lv_label_create(page);
	lv_label_set_long_mode(rx_uuid_lbl, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_align(rx_uuid_lbl, LV_TEXT_ALIGN_RIGHT, 0);
	lv_obj_set_style_text_font(rx_uuid_lbl, &lv_font_montserrat_24, LV_PART_MAIN);
	lv_obj_set_pos(rx_uuid_lbl, 0, row_y);
	lv_obj_set_width(rx_uuid_lbl, screen_w/2 - 5);
	lv_label_set_text_static(rx_uuid_lbl, "RX Char UUID");
	lv_obj_add_flag(rx_uuid_lbl, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_add_event_cb(rx_uuid_lbl, _gui_screen_ble_val_cb, LV_EVENT_CLICKED, NULL);
	
	// Create the SSID value label
	rx_uuid = lv_label_create(page);
	lv_label_set_long_mode(rx_uuid, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_align(rx_uuid, LV_TEXT_ALIGN_LEFT, 0);
	lv_obj_set_style_text_color(rx_uuid, lv_palette_main(LV_PALETTE_LIGHT_BLUE), LV_PART_MAIN);
	lv_obj_set_style_text_font(rx_uuid, &lv_font_montserrat_24, LV_PART_MAIN);
	lv_obj_set_pos(rx_uuid, screen_w/2 + 5, row_y);
	lv_obj_set_width(rx_uuid, screen_w/2 - 5);
	lv_obj_add_flag(rx_uuid, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_add_event_cb(rx_uuid, _gui_screen_ble_val_cb, LV_EVENT_CLICKED, NULL);
	
	row_y += vertical_spacing;
}


static void _gui_screen_ble_setup_buttons()
{
	uint16_t btn_w = screen_w / 4;
	uint16_t btn_h = screen_h / 10;
	
	// Cancel button and label
	cancel_btn = lv_btn_create(page);
	lv_obj_set_size(cancel_btn, btn_w, btn_h);
	lv_obj_set_pos(cancel_btn, screen_w/2 - btn_w - btn_w/3, row_y);
	lv_obj_add_event_cb(cancel_btn, _gui_screen_ble_btn_cb, LV_EVENT_ALL, NULL);
	
	cancel_btn_lbl = lv_label_create(cancel_btn);
	lv_obj_set_style_text_font(cancel_btn_lbl, &lv_font_montserrat_30, LV_PART_MAIN);
	lv_label_set_text_static(cancel_btn_lbl, "Cancel");
	lv_obj_center(cancel_btn_lbl);
	
	// Save button and label
	save_btn = lv_btn_create(page);
	lv_obj_set_size(save_btn, btn_w, btn_h);
	lv_obj_set_pos(save_btn, screen_w/2 + btn_w/3, row_y);
	lv_obj_add_event_cb(save_btn, _gui_screen_ble_btn_cb, LV_EVENT_ALL, NULL);
	
	save_btn_lbl = lv_label_create(save_btn);
	lv_obj_set_style_text_font(save_btn_lbl, &lv_font_montserrat_30, LV_PART_MAIN);
	lv_label_set_text_static(save_btn_lbl, "Save");
	lv_obj_center(save_btn_lbl);
	
	row_y += vertical_spacing;
}


static void _gui_screen_update_uuid_strings()
{
		if (cur_custom_enable) {
			lv_label_set_text_static(service_uuid, cur_service_uuid_str);
			lv_label_set_text_static(tx_uuid, cur_tx_char_uuid_str);
			lv_label_set_text_static(rx_uuid, cur_rx_char_uuid_str);
		} else {
			lv_label_set_text_static(service_uuid, "----");
			lv_label_set_text_static(tx_uuid, "----");
			lv_label_set_text_static(rx_uuid, "----");
		}

}


static void _gui_screen_ble_enable_cb(lv_event_t* e)
{
	lv_event_code_t code = lv_event_get_code(e);
	lv_obj_t* obj = lv_event_get_target(e);
	
	if (code == LV_EVENT_VALUE_CHANGED) {
		if (obj == enable_custom_sw) {
			cur_custom_enable = lv_obj_has_state(obj, LV_STATE_CHECKED);
			
			_gui_screen_update_uuid_strings();
		}
	}
}


static void _gui_screen_ble_val_cb(lv_event_t* e)
{
	lv_event_code_t code = lv_event_get_code(e);
	lv_obj_t* obj = lv_event_get_target(e);
	
	// These controls are only active with a custom interface selection
	if ((code == LV_EVENT_CLICKED) && cur_custom_enable) {
		// Display a keyboard with the selected value to edit
		if ((obj == service_uuid_lbl) || (obj == service_uuid)) {
			gui_utility_display_hex_kbd(page, "Service UUID", VAL_SERVICE_INDEX, cur_service_uuid_str, MAX_PORT_DIGITS, &_gui_screen_ble_update_textfield);
		} else if ((obj == tx_uuid_lbl) || (obj == tx_uuid)) {
			gui_utility_display_hex_kbd(page, "TX Char UUID", VAL_TX_UUID_INDEX, cur_tx_char_uuid_str, MAX_PORT_DIGITS, &_gui_screen_ble_update_textfield);
		} else if ((obj == rx_uuid_lbl) || (obj == rx_uuid)) {
			gui_utility_display_hex_kbd(page, "RX Char UUID", VAL_RX_UUID_INDEX, cur_rx_char_uuid_str, MAX_PORT_DIGITS, &_gui_screen_ble_update_textfield);
		}
	}
}


static void _gui_screen_ble_btn_cb(lv_event_t* e)
{
	bool changed = false;
	
	lv_event_code_t code = lv_event_get_code(e);
	lv_obj_t* obj = lv_event_get_target(e);
	
	if (code == LV_EVENT_CLICKED) {
		if (obj == cancel_btn) {
			// Return to settings display with no changes
			gui_set_screen_page(GUI_SCREEN_MAIN);
		} else if (obj == save_btn) {
			// Determine if there are any changes
			if (cur_custom_enable != configP->use_custom_uuid) {
				configP->use_custom_uuid = cur_custom_enable;
				changed = true;
			}
			
			// Only save changed UUIDs if enabled
			if (cur_custom_enable) {
				if (strcmp(cur_service_uuid_str, configP->service_uuid) != 0) {
					strcpy(configP->service_uuid, cur_service_uuid_str);
					changed = true;
				}
				
				if (strcmp(cur_tx_char_uuid_str, configP->tx_char_uuid) != 0) {
					strcpy(configP->tx_char_uuid, cur_tx_char_uuid_str);
					changed = true;
				}
				
				if (strcmp(cur_rx_char_uuid_str, configP->rx_char_uuid) != 0) {
					strcpy(configP->rx_char_uuid, cur_rx_char_uuid_str);
					changed = true;
				}
			}
			
			// Save to persistent storage if necessary
			if (changed) {
				if (ps_save_config(PS_CONFIG_TYPE_BLE)) {
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


static void _gui_screen_ble_update_textfield(int index, char* val)
{
	switch (index) {
		case VAL_SERVICE_INDEX:
			strcpy(cur_service_uuid_str, val);
			lv_label_set_text_static(service_uuid, cur_service_uuid_str);
			break;
		
		case VAL_TX_UUID_INDEX:
			strcpy(cur_tx_char_uuid_str, val);
			lv_label_set_text_static(tx_uuid, cur_tx_char_uuid_str);
			break;
		
		case VAL_RX_UUID_INDEX:
			strcpy(cur_rx_char_uuid_str, val);
			lv_label_set_text_static(rx_uuid, cur_rx_char_uuid_str);
			break;
	}
}
