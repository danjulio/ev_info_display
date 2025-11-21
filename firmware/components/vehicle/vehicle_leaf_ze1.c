/*
 * Nissan Leaf ZE1 (2018-2025) platform vehicle implementation
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
#include "vehicle_leaf_ze1.h"
#include "can_manager.h"
#include "data_broker.h"
#include "esp_system.h"
#include "esp_log.h"


//
// Local constants
//

// Uncomment to debug
//#define DEBUG_DATA

// CAN UDS request list indicies
#define UDS_GEAR_POSITION 0
#define UDS_12V_BATT_V    1
#define UDS_12V_BATT_I    2
#define UDS_LV_AUX_PWR    3
#define UDS_AC_AUX_PWR    4
#define UDS_SPEED         5
#define UDS_HV_BATT_INFO  6
#define UDS_HV_BATT_TEMP  7
#define UDS_TORQUE        8

#define NUM_UDS_REQ_ITEMS 9


// Gear position constants
#define GEAR_PARK         1
#define GEAR_REVERSE      2
#define GEAR_NEUTRAL      3
#define GEAR_DRIVE        4
#define GEAR_DRIVE_ECO    5


//
//  Forward declarations
//

// Functions for vehicle manager
static void _leaf_ze1_init();
static void _leaf_ze1_eval();
static void _leaf_ze1_set_req_mask(uint32_t mask);
static void _leaf_ze1_rx_data(uint32_t id, int len, uint8_t* data);
static void _leaf_ze1_error(int errno);

// Internal functions
static float _leaf_ze1_hv_batt_raw_to_f(int16_t raw);


//
// Vehicle definitions
//
const vehicle_config_t vehicle_leaf_ze1 =
{
	"Leaf ZE1",
	DB_ITEM_HV_BATT_V | DB_ITEM_HV_BATT_I | DB_ITEM_HV_BATT_MIN_T | DB_ITEM_HV_BATT_MAX_T | \
	DB_ITEM_LV_BATT_V | DB_ITEM_LV_BATT_I | \
	DB_ITEM_AUX_KW | DB_ITEM_FRONT_TORQUE | \
	DB_ITEM_SPEED,
	{-40.0, 160.0},     // power_kw_range - 
	{0.0, 8.0},         // aux_kw_range
	{-100.0, 250.0},    // torque_nm_range
	{-150.0, 450.0},    // hv_batt_i_range
	{10.0, 16.0},       // lv_batt_v_range
	true,               // 500k CAN
	500,                // Request timeout (mSec)
	_leaf_ze1_init,
	_leaf_ze1_eval,
	_leaf_ze1_set_req_mask,
	_leaf_ze1_rx_data,
	_leaf_ze1_error
};



//
// Vehicle UDS service CAN request packets (must match list of indicies)
//
//                                                 Req ID      Rsp ID         PCI   SID
static const can_request_t req_gear_position   = {     0x797,      0x79A, 8, {0x03, 0x22, 0x11, 0x56, 0x00, 0x00, 0x00, 0x00}};
static const can_request_t req_12v_batt_v      = {     0x797,      0x79A, 8, {0x03, 0x22, 0x11, 0x03, 0x00, 0x00, 0x00, 0x00}};
static const can_request_t req_12v_batt_i      = {     0x797,      0x79A, 8, {0x03, 0x22, 0x11, 0x83, 0x00, 0x00, 0x00, 0x00}};
static const can_request_t req_lv_aux_pwr      = {     0x797,      0x79A, 8, {0x03, 0x22, 0x11, 0x52, 0x00, 0x00, 0x00, 0x00}};
static const can_request_t req_ac_aux_pwr      = {     0x797,      0x79A, 8, {0x03, 0x22, 0x11, 0x51, 0x00, 0x00, 0x00, 0x00}};
static const can_request_t req_speed           = {     0x797,      0x79A, 8, {0x03, 0x22, 0x12, 0x1A, 0x00, 0x00, 0x00, 0x00}};
static const can_request_t req_hv_batt_info    = {     0x79B,      0x7BB, 8, {0x02, 0x21, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00}};
static const can_request_t req_hv_batt_temp    = {     0x79B,      0x7BB, 8, {0x02, 0x21, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00}};
static const can_request_t req_torque          = {     0x784,      0x78C, 8, {0x03, 0x22, 0x12, 0x25, 0x00, 0x00, 0x00, 0x00}};

static const can_request_t* req_full_listP[] = {
	&req_gear_position,
	&req_12v_batt_v,
	&req_12v_batt_i,
	&req_lv_aux_pwr,
	&req_ac_aux_pwr,
	&req_speed,
	&req_hv_batt_info,
	&req_hv_batt_temp,
	&req_torque,
};



//
// Global variables
//
static const char* TAG = "vehicle_leaf_ze1";

// OBD2 Request management
static can_request_t* req_listP[NUM_UDS_REQ_ITEMS];
static bool req_in_process = false;
static bool req_timeout = false;
static bool saw_error = false;
static bool saw_response = false;
static int req_index = 0;
static int num_req_entries = 0;

// Partial data values
static bool in_reverse = false;
static float lv_aux_kw = 0;
static float ac_aux_kw = 0;
static float hv_batt_t[4] = {0.0, 0.0, 0.0, 0.0};



//
// Vehicle manager functions
//
static void _leaf_ze1_init()
{
	// We don't need to filter the OBD CAN bus because the car's gateway does that for us
	can_en_rsp_filter(false);
}


static void _leaf_ze1_eval()
{
	// Handle any responses or timeout
	if (req_in_process) {
		if (saw_error || saw_response || req_timeout) {
			req_in_process = false;
			
			if (req_timeout) {
				req_timeout = false;
				ESP_LOGI(TAG, "Request timeout");
			}
		}
	}
	
	// Look to see if we can make a request
	if (!req_in_process && (num_req_entries > 0)) {
		req_in_process = true;
		saw_response = false;
		req_timeout = false;

#ifdef DEBUG_DATA
			ESP_LOGI(TAG, "TX: id = %lx, rsp = %lx, len = %d", req_listP[req_index]->req_id, req_listP[req_index]->rsp_id, req_listP[req_index]->req_len);
#endif

		if (can_tx_packet(req_listP[req_index]->req_id, req_listP[req_index]->rsp_id, req_listP[req_index]->req_len, req_listP[req_index]->data)) {
			saw_error = false;
		} else {
			saw_error = true;
			ESP_LOGE(TAG, "CAN TX fail - ID: %x", req_listP[req_index]->req_id);
		}
		
		// Setup for next request
		req_index += 1;
		if (req_index >= num_req_entries) {
			req_index = 0;
		}
	}
}


static void _leaf_ze1_set_req_mask(uint32_t mask)
{
	bool required_req[NUM_UDS_REQ_ITEMS];
	int n = 0;
	
	// Determine what requests are necessary
	required_req[UDS_GEAR_POSITION] = vm_mask_check(mask, DB_ITEM_FRONT_TORQUE);
	required_req[UDS_12V_BATT_V]    = vm_mask_check(mask, DB_ITEM_LV_BATT_V);
	required_req[UDS_12V_BATT_I]    = vm_mask_check(mask, DB_ITEM_LV_BATT_I);
	required_req[UDS_LV_AUX_PWR]    = vm_mask_check(mask, DB_ITEM_AUX_KW);
	required_req[UDS_AC_AUX_PWR]    = vm_mask_check(mask, DB_ITEM_AUX_KW);
	required_req[UDS_SPEED]         = vm_mask_check(mask, DB_ITEM_SPEED);
	required_req[UDS_HV_BATT_INFO]  = vm_mask_check(mask, DB_ITEM_HV_BATT_V | DB_ITEM_HV_BATT_I);
	required_req[UDS_HV_BATT_TEMP]  = vm_mask_check(mask, DB_ITEM_HV_BATT_MIN_T | DB_ITEM_HV_BATT_MAX_T);
	required_req[UDS_TORQUE]        = vm_mask_check(mask, DB_ITEM_FRONT_TORQUE);
	
	// Build up our list of requests and reset starting point
	num_req_entries = 0;
	req_index = 0;
	for (int i=0; i<NUM_UDS_REQ_ITEMS; i++) {
		if (required_req[i]) {
			num_req_entries += 1;
			req_listP[n++] = (can_request_t*) req_full_listP[i];
		}
	}
}


static void _leaf_ze1_rx_data(uint32_t id, int len, uint8_t* data)
{
	float f;
	int n;
	int16_t i16;
	int32_t i32;
	uint16_t u16;
	
	saw_response = true;
	
#ifdef DEBUG_DATA
	ESP_LOGI(TAG, "RX: id = %lx, len = %d", id, len);
#endif
	
	// Try to find the request this response matches
	n = vm_get_resp_index(id, len, data, NUM_UDS_REQ_ITEMS, req_full_listP);
	
	switch (n) {
		case UDS_GEAR_POSITION:
			if (len == 4) {
				in_reverse = (data[3] == GEAR_REVERSE);
			}
			break;
		
		case UDS_12V_BATT_V:
			if (len == 4) {
				f = (float) data[3] * 0.08;
				vm_update_data_item(DB_ITEM_LV_BATT_V, f);
			}
			break;
		
		case UDS_12V_BATT_I:
			if (len == 5) {
				i16 = (data[3] << 8) | data[4];
				f = (float) i16 / 256.0;
				vm_update_data_item(DB_ITEM_LV_BATT_I, f);
			}
			break;
		
		case UDS_LV_AUX_PWR:
			if (len == 4) {
				lv_aux_kw = (float) data[3] * 0.1;
				f = lv_aux_kw + ac_aux_kw;
				vm_update_data_item(DB_ITEM_AUX_KW, f);
			}
			break;
		
		case UDS_AC_AUX_PWR:
			if (len == 4) {
				ac_aux_kw = (float) data[3] * 0.250;
				f = lv_aux_kw + ac_aux_kw;
				vm_update_data_item(DB_ITEM_AUX_KW, f);
			}
			break;
		
		case UDS_SPEED:
			if (len == 5) {
				u16 = (data[3] << 8) | data[4];
				f = (float) u16 / 10.0;
				vm_update_data_item(DB_ITEM_SPEED, f);
			}
			break;
		
		case UDS_HV_BATT_INFO:
			if (len == 53) {
/*
				i32 = (data[2] << 24) | (data[3] << 16) | (data[4] << 8) | data[5];
				f = (float) i32 / 1024.0;
				ESP_LOGI(TAG, "Batt Current 1 = %1.2f", f);
				vm_update_data_item(DB_ITEM_HV_BATT_I, f);
*/
				// Battery current 2 seems to be a more accurate average value
				i32 = (data[8] << 24) | (data[9] << 16) | (data[10] << 8) | data[11];
				f = (float) i32 / 1024.0;
//				ESP_LOGI(TAG, "Batt Current 2 = %1.2f", f);
				vm_update_data_item(DB_ITEM_HV_BATT_I, f);
				
				u16 = (data[20] << 8) | data[21];
				f = (float) u16 / 100;
				vm_update_data_item(DB_ITEM_HV_BATT_V, f);
			}
			break;
		
		case UDS_HV_BATT_TEMP:
			if (len == 31) {
				i16 = (data[2] << 8) | data[3];
				hv_batt_t[0] = ((_leaf_ze1_hv_batt_raw_to_f(i16) - 32.0) * 5.0) / 9.0;
				i16 = (data[5] << 8) | data[6];
				hv_batt_t[1] = ((_leaf_ze1_hv_batt_raw_to_f(i16) - 32.0) * 5.0) / 9.0;
/*
				// Not used in ZE1
				i16 = (data[8] << 8) | data[9];
				hv_batt_t[2] = ((_leaf_ze1_hv_batt_raw_to_f(i16) - 32.0) * 5.0) / 9.0;
*/
				i16 = (data[11] << 8) | data[12];
				hv_batt_t[3] = ((_leaf_ze1_hv_batt_raw_to_f(i16) - 32.0) * 5.0) / 9.0;
				
				// Find min
				if (hv_batt_t[1] < hv_batt_t[0]) {
					if (hv_batt_t[3] < hv_batt_t[1]) {
						f = hv_batt_t[3];
					} else {
						f = hv_batt_t[1];
					}
				} else {
					if (hv_batt_t[3] < hv_batt_t[0]) {
						f = hv_batt_t[3];
					} else {
						f = hv_batt_t[0];
					}
				}
				vm_update_data_item(DB_ITEM_HV_BATT_MIN_T, f);
				
				// Find max
				if (hv_batt_t[1] > hv_batt_t[0]) {
					if (hv_batt_t[3] > hv_batt_t[1]) {
						f = hv_batt_t[3];
					} else {
						f = hv_batt_t[1];
					}
				} else {
					if (hv_batt_t[3] > hv_batt_t[0]) {
						f = hv_batt_t[3];
					} else {
						f = hv_batt_t[0];
					}
				}
				vm_update_data_item(DB_ITEM_HV_BATT_MAX_T, f);
			}
			break;
		
		case UDS_TORQUE:
			if (len == 5) {
				i16 = (data[3] << 8) | data[4];
				f = (float) i16 / 64.0;
				
				// For the ZE1 the torque value is the actual torque going to the motor
				// which means going in reverse is the same as applying regen while going forward
				// so we use knowledge of the shift position to negate the torque for reverse
				// so it still shows up as a positive number (a request to move the car as opposed
				// to regenerate energy back into the battery)
				if (in_reverse) {
					f = -f;
				}
				vm_update_data_item(DB_ITEM_FRONT_TORQUE, f);
			}
			break;
	}
}


static void _leaf_ze1_error(int errno)
{
	// We only handle (and expect) timeouts
	if (errno == CAN_ERRNO_TIMEOUT) {
		req_timeout = true;
	}
}



//
// Internal functions
//
static float _leaf_ze1_hv_batt_raw_to_f(int16_t raw)
{
	if (raw == 1021) {
		return 1.0;
	} else if (raw >= 589) {
		return 162.0 - ((float) raw * 0.181);
	} else if (raw >= 569) {
		return 57.2 + ((float) (579 - raw) * 0.18);
	} else if (raw >= 558) {
		return 60.8 + ((float) (558 - raw) * 0.16363636363636364);
	} else if (raw >= 548) {
		return 62.6 + ((float) (548 - raw) * 0.18);
	} else if (raw >= 537) {
		return 64.4 + ((float) (537 - raw) * 0.16363636363636364);
	} else if (raw >= 447) {
		return 66.2 + ((float) (527 - raw) * 0.18);
	} else if (raw >= 438) {
		return 82.4 + ((float) (438 - raw) * 0.2);
	} else if (raw >= 428) {
		return 84.2 + ((float) (428 - raw) * 0.18);
	} else if (raw >= 365) {
		return 86.0 + ((float) (419 - raw) * 0.2);
	} else if (raw >= 357) {
		return 98.6 + ((float) (357 - raw) * 0.225);
	} else if (raw >= 348) {
		return 100.4 + ((float) (348 - raw) * 0.2);
	} else if (raw >= 316) {
		return 102.2 + ((float) (340 - raw) * 0.225);
	} else {
		return 109.4 + ((float) (309 - raw) * 0.2571428571428572);
	}
}