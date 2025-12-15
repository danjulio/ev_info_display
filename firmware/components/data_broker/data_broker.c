/*
 * Data Broker
 *
 * Provide interface between GUI and Vehicle data collection.
 *  - Define data objects
 *  - Provide interface between vehicle task and gui task
 *  - GUI screens register callbacks for specific data items
 *  - Vehicle manager send data item/value pairs for GUI updates
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
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdio.h>


//
// Module variables
//
static bool gui_item_fast_average = false;
static gui_item_value_handler gui_handler_list[DB_MAX_ITEMS];
static float gui_item_value_list[2][DB_MAX_ITEMS];
static uint32_t gui_item_updated_mask;

static SemaphoreHandle_t update_mutex;


//
// Forward declarations
//
static int _db_mask_to_index(uint32_t mask);



//
// API
//
esp_err_t db_init()
{
	for (int i=0; i<DB_MAX_ITEMS; i++) {
		gui_handler_list[i] = NULL;
		gui_item_value_list[0][i] = 0;
	}
	
	update_mutex = xSemaphoreCreateMutex();
	return (update_mutex == NULL) ? ESP_FAIL : ESP_OK;
}


void db_enable_fast_average(bool en)
{
	gui_item_fast_average = true;
}


void db_gui_eval()
{
	xSemaphoreTake(update_mutex, portMAX_DELAY);
	for (int i=0; i<DB_MAX_ITEMS; i++) {
		if ((gui_item_updated_mask & (1 << i)) != 0) {
			if (gui_handler_list[i] != NULL) {
				if (gui_item_fast_average) {
					gui_handler_list[i]((gui_item_value_list[0][i] + gui_item_value_list[1][i])/2.0);
				} else {
					gui_handler_list[i](gui_item_value_list[0][i]);
				}
			}
		}
	}
	gui_item_updated_mask = 0;
	xSemaphoreGive(update_mutex);
}


void db_register_gui_callback(uint32_t mask, gui_item_value_handler fcn)
{
	int n;
	
	n = _db_mask_to_index(mask);
	
	if (n >= 0) {
		gui_handler_list[n] = fcn;
		gui_item_updated_mask &= ~(1 << n);
		gui_item_value_list[0][n] = 0;
	}
}


void db_set_data_item_value(uint32_t mask, float val)
{
	int n;
	
	n = _db_mask_to_index(mask);
	
	xSemaphoreTake(update_mutex, portMAX_DELAY);
	if (n >= 0) {
		gui_item_updated_mask |= (1 << n);
		gui_item_value_list[1][n] = gui_item_value_list[0][n];
		gui_item_value_list[0][n] = val;
	}
	xSemaphoreGive(update_mutex);
}



//
// Internal functions
//

// Returns -1 if no valid bit found, otherwise returns the first bit found from LSb
static int _db_mask_to_index(uint32_t mask)
{
	int n = 0;
	uint32_t shift_mask = 0x00000001;
	
	// Return first set bit position
	while (n<DB_MAX_ITEMS) {
		if ((mask & shift_mask) != 0) {
			return n;
		}
		n += 1;
		shift_mask <<= 1;
	}
	
	return -1;
}