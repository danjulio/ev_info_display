/*
 * Vehicle Manager
 *
 * Manager selected vehicle data
 *   - Vehicle type data structure definition
 *   - Manage vehicle type
 *   - Interface for GUI to determine supported data items, and item ranges as necessary
 *   - Interface for CAN manager to provide specific vehicle logic with data
 *   - Interface for controlling task to evaluate vehicle data collection
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
#include "data_broker.h"
#include "esp_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "vehicle_manager.h"
#include "vehicle_leaf_ze1.h"
#include "vehicle_vw_meb.h"
#include <string.h>



//
// List of all implemented vehicle types
//
#define NUM_VEHICLES 3
static const vehicle_config_t* vehicle_listP[NUM_VEHICLES] =
{
	&vehicle_leaf_ze1,
	&vehicle_vw_meb_awd,
	&vehicle_vw_meb_rwd
};



//
// Module variables
//
static const char* TAG = "vehicle_manager";

static vehicle_config_t* cur_vehicleP = NULL;

// Asynchronous update of request mask from GUI
static bool update_req_mask_flag = false;
static uint32_t new_req_mask;

// Response
static bool rx_data_valid = false;
static int rx_data_len;
static uint8_t rx_data_buf[4096];
static uint32_t rx_id;



//
// API
//
bool vm_init(const char* vehicle_name, int if_type)
{
	// Try to find the vehicle
	for (int i=0; i<NUM_VEHICLES; i++) {
		if (strcmp(vehicle_listP[i]->name, vehicle_name) == 0) {
			cur_vehicleP = (vehicle_config_t*) vehicle_listP[i];
			
			// First, initialize the interface
			if (can_init(if_type, cur_vehicleP->req_timeout_msec, cur_vehicleP->can_is_500k)) {
				// Then initialize the vehicle
				cur_vehicleP->fcn_init();
				
				return true;
			} else {
				return false;
			}
		}
	}
	
	return false;
}


void vm_eval()
{
	if (cur_vehicleP != NULL) {
		// Process any received data
		if (rx_data_valid) {
			cur_vehicleP->fcn_rx_data(rx_id, rx_data_len, rx_data_buf);
			rx_data_valid = false;
		}
		
		// Look for updated request mask
		if (update_req_mask_flag) {
			update_req_mask_flag = false;
			cur_vehicleP->fcn_set_req_mask(new_req_mask);
		}
		
		// Then allow the vehicle to evaluate
		cur_vehicleP->fcn_eval();
	}
}


// Returns -1 for no match
int vm_get_resp_index(uint32_t resp_can_id, int resp_data_len, uint8_t* resp_data, int req_list_len, const can_request_t* req_list[])
{
	bool match;
	int i, j;
	int n;
	
	// Must at least have a UDS packet length (byte 0) and service ID (byte 1)
	if (resp_data_len < 2) {
		return -1;
	}
	
	// Must not be a negative response
	if (resp_data[0] == 0x7F) {
		return -1;
	}
	
	// Look for a match in the list of requests
	for (i=0; i<req_list_len; i++) {
		// Check CAN ID and SID
		match = (resp_can_id == req_list[i]->rsp_id) && (resp_data[0] == (req_list[i]->data[1] + 0x40));
		
		// Check remaining request bytes
		if (match) {
			if (resp_data_len <= req_list[i]->data[0]) {
				// Not enough incoming data to even check against original request
				match = false;
			} else {
				n = req_list[i]->data[0] - 1;  // Count of additional subfunction/DID bytes in request (beyond SID)
				j = 2;
				while (n--) {
					match &= (resp_data[j-1] == req_list[i]->data[j]);
					j += 1;
				}
			}
		}
		
		if (match) {
			return i;
		}
	}
	
	
	return -1;
}


void vm_update_data_item(uint32_t mask, float val)
{
	db_set_data_item_value(mask, val);
}


bool vm_mask_check(uint32_t req_mask, uint32_t mask_list)
{
	return (req_mask & mask_list) != 0;
}


// May be called from within an ISR context so we copy data to a local buffer
// for processing by a task later
void vm_rx_data(uint32_t id, int len, uint8_t* data)
{
	if ((cur_vehicleP != NULL) && (!rx_data_valid)) {
		rx_id = id;
		rx_data_len = len;
		memcpy(rx_data_buf, data, (size_t) len);
		rx_data_valid = true;
	}
}


void vm_note_error(int errno)
{
	if (cur_vehicleP != NULL) {
		cur_vehicleP->fcn_note_can_error(errno);
	}
}


int vm_get_num_vehicles()
{
	return NUM_VEHICLES;
}


const char* vm_get_vehicle_name(int n)
{
	if ((n >= 0) && (n < NUM_VEHICLES)) {
		return vehicle_listP[n]->name;
	}
	
	return NULL;
}


uint32_t vm_get_supported_item_mask()
{
	if (cur_vehicleP != NULL) {
		return cur_vehicleP->supported_item_mask;
	}
	
	return 0;
}


void vm_set_request_item_mask(uint32_t mask)
{
	new_req_mask = mask;
	update_req_mask_flag = true;
}


bool vm_get_range(int index, float* min, float* max)
{
	bool success = true;
	
	if (cur_vehicleP != NULL) {
		switch (index) {
			case VM_RANGE_POWER:
				*min = cur_vehicleP->power_kw_range.min;
				*max = cur_vehicleP->power_kw_range.max;
				break;
			case VM_RANGE_AUX:
				*min = cur_vehicleP->aux_kw_range.min;
				*max = cur_vehicleP->aux_kw_range.max;
				break;
			case VM_RANGE_TORQUE:
				*min = cur_vehicleP->torque_nm_range.min;
				*max = cur_vehicleP->torque_nm_range.max;
				break;
			case VM_RANGE_HV_BATTI:
				*min = cur_vehicleP->hv_batt_i_range.min;
				*max = cur_vehicleP->hv_batt_i_range.max;
				break;
			case VM_RANGE_LV_BATTV:
				*min = cur_vehicleP->lv_batt_v_range.min;
				*max = cur_vehicleP->lv_batt_v_range.max;
				break;
			default:
				success = false;
		}
	} else {
		success = false;
	}
	
	return success;
}
