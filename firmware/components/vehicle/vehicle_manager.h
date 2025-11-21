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
#ifndef VEHICLE_MANAGER_H
#define VEHICLE_MANAGER_H

#include <stdbool.h>
#include <stdint.h>



//
// Global constants
//

// Indicies for accessing vehicle ranges
// Notes about ranges for best display gauge layout
//   1. Ranges should be even, whole numbers
//   2. POWER specific: should be multiples of 10
//   3. AUX specific: min delta should be 8 kW
//   4. ???
#define VM_RANGE_POWER    0
#define VM_RANGE_AUX      1
#define VM_RANGE_TORQUE   2
#define VM_RANGE_HV_BATTI 3
#define VM_RANGE_LV_BATTV 4



//
// Vehicle module specific evaluation functions
//
typedef void (*vehicle_init)();
typedef void (*vehicle_eval)();
typedef void (*vehicle_set_req_mask)(uint32_t mask);
typedef void (*vehicle_rx_data)(uint32_t id, int len, uint8_t* data);
typedef void (*vehicle_note_can_error)(int errno);



//
// Global Data structures
//

// Vehicle UDS service CAN request packets for each mask
typedef struct {
	uint32_t req_id;            // Request CAN ID
	uint32_t rsp_id;            // Response CAN ID
	int req_len;                // Number of valid bytes in the request
	uint8_t data[];             // CAN Data
} can_request_t;

// Vehicle configuration
typedef struct {
	float min;
	float max;
} item_range_t;

typedef struct {
	char* name;
	uint32_t supported_item_mask;
	item_range_t power_kw_range;
	item_range_t aux_kw_range;
	item_range_t torque_nm_range;
	item_range_t hv_batt_i_range;
	item_range_t lv_batt_v_range;
	bool can_is_500k;
	int req_timeout_msec;                          // CAN Bus Request->Response timeout
	vehicle_init fcn_init;
	vehicle_eval fcn_eval;
	vehicle_set_req_mask fcn_set_req_mask;
	vehicle_rx_data fcn_rx_data;
	vehicle_note_can_error fcn_note_can_error;
} vehicle_config_t;



//
// API
//

// For vehicle_task
bool vm_init(const char* vehicle_name, int if_type);
void vm_eval();

// For vehicle implementations
int vm_get_resp_index(uint32_t resp_can_id, int resp_data_len, uint8_t* resp_data, int req_list_len, const can_request_t* req_list[]);
void vm_update_data_item(uint32_t mask, float val);
bool vm_mask_check(uint32_t req_mask, uint32_t mask_list);

// For CAN manager
void vm_rx_data(uint32_t id, int len, uint8_t* data);
void vm_note_error(int errno);

// For vehicle_task and GUI use
int vm_get_num_vehicles();
const char* vm_get_vehicle_name(int n);

// For GUI use
uint32_t vm_get_supported_item_mask();
void vm_set_request_item_mask(uint32_t mask);
bool vm_get_range(int index, float* min, float* max);

#endif /* VEHICLE_MANAGER_H */