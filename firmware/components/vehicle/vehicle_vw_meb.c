/*
 * Volkswagen MEB platform vehicle implementation
 *
 * Copyright 2025-2026 Dan Julio
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
#include "vehicle_vw_meb.h"
#include "can_manager.h"
#include "data_broker.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"


//
// Local constants
//

// Uncomment to debug
//#define DEBUG_DATA

// CAN UDS request list indicies
#define UDS_12V_BATT_INFO 0
#define UDS_GPS_INFO      1
#define UDS_HV_AUX_PWR    2
#define UDS_HV_BATT_CUR   3
#define UDS_HV_BATT_MIN_T 4
#define UDS_HV_BATT_MAX_T 5
#define UDS_HV_BATT_VOLT  6
#define UDS_FRONT_TORQUE  7
#define UDS_REAR_TORQUE   8
#define UDS_GEAR_POSITION 9
#define UDS_SPEED         10
#define UDS_HV_CELL_VOLT  11
#define UDS_CAR_MODE      12

#define NUM_UDS_REQ_ITEMS 13

// Gear position constants
#define GEAR_PARK         0x08
#define GEAR_REVERSE      0x07
#define GEAR_NEUTRAL      0x06
#define GEAR_DRIVE_D      0x05
#define GEAR_DRIVE_B      0x0C

// Car Mode constants
#define MODE_STANDBY      0x00
#define MODE_DRIVING      0x01
#define MODE_AC_CHARGE    0x04
#define MODE_DC_CHARGE    0x06

// Car Mode request interval.  This is based on my 2024 ID.4 period of about 11 seconds
// between car mode returning to STANDBY and the gateway stopping replying to my requests.
// We want this to be slow so we don't slow down displayed requests (e.g. in the speed test tile).
#define CAR_MODE_REQ_USEC (5000 * 1000)

// GPS Info request interval.  The GPS in my ID.4 does not appear to be happy with too many
// requests, returning time-outs and negative responses.  Since it's not a datum that requires
// fast updating, we slow it down to an interval that, at least, eliminates the timeouts.
#define GPS_REQ_USEC      (1500 * 1000)


//
//  Forward declarations
//

// Functions for vehicle manager
static void _vw_meb_init(int if_type);
static void _vw_meb_eval();
static void _vw_meb_set_req_mask(uint32_t mask);
static void _vw_meb_rx_data(uint32_t id, int len, uint8_t* data);
static void _vw_meb_error(int errno);



//
// Vehicle definitions
//
// ID.3 Pure
const vehicle_config_t vehicle_vw_meb84_rwd = 
{
	"VW MEB84 RWD",
	DB_ITEM_HV_BATT_V | DB_ITEM_HV_BATT_I | DB_ITEM_HV_BATT_MIN_T | DB_ITEM_HV_BATT_MAX_T | \
	DB_ITEM_LV_BATT_V | DB_ITEM_LV_BATT_I | \
	DB_ITEM_AUX_KW | DB_ITEM_REAR_TORQUE | \
	DB_ITEM_SPEED | DB_ITEM_GPS_ELEVATION | \
	DB_ITEM_HV_CELL_V,
	84,                      // Number of cells in HV battery
	{-150.0, 200.0, 50.0},   // power_kw_range
	{0.0, 16.0, 3.0},        // aux_kw_range
	{-200.0, 400.0, 50.0},   // torque_nm_range
	{-300.0, 500.0, 100.0},  // hv_batt_i_range
	{10.0, 16.0, 2.0},       // lv_batt_v_range
	{3.0, 4.2, 0.2},         // hv_batt_cell_range
	true,                    // 500k CAN
	500,                     // Request timeout (mSec)
	_vw_meb_init,
	_vw_meb_eval,
	_vw_meb_set_req_mask,
	_vw_meb_rx_data,
	_vw_meb_error
};

// ID.3 Pure Performance, ID.4, ID.5, ID.Buzz SWB RWD
const vehicle_config_t vehicle_vw_meb96_rwd =
{
	"VW MEB RWD",
	DB_ITEM_HV_BATT_V | DB_ITEM_HV_BATT_I | DB_ITEM_HV_BATT_MIN_T | DB_ITEM_HV_BATT_MAX_T | \
	DB_ITEM_LV_BATT_V | DB_ITEM_LV_BATT_I | \
	DB_ITEM_AUX_KW | DB_ITEM_REAR_TORQUE | \
	DB_ITEM_SPEED | DB_ITEM_GPS_ELEVATION | \
	DB_ITEM_HV_CELL_V,
	96,                      // Number of cells in HV battery
	{-200.0, 300.0, 50.0},   // power_kw_range
	{0.0, 16.0, 3.0},        // aux_kw_range
	{-200.0, 500.0, 100.0},  // torque_nm_range
	{-400.0, 600.0, 100.0},  // hv_batt_i_range
	{10.0, 16.0, 2.0},       // lv_batt_v_range
	{3.0, 4.2, 0.2},         // hv_batt_cell_range
	true,                    // 500k CAN
	500,                     // Request timeout (mSec)
	_vw_meb_init,
	_vw_meb_eval,
	_vw_meb_set_req_mask,
	_vw_meb_rx_data,
	_vw_meb_error
};

// ID.3 Pure Performance, ID.4, ID.5, ID.Buzz SWB AWD
const vehicle_config_t vehicle_vw_meb96_awd =
{
	"VW MEB AWD",
	DB_ITEM_HV_BATT_V | DB_ITEM_HV_BATT_I | DB_ITEM_HV_BATT_MIN_T | DB_ITEM_HV_BATT_MAX_T | \
	DB_ITEM_LV_BATT_V | DB_ITEM_LV_BATT_I | \
	DB_ITEM_AUX_KW | DB_ITEM_FRONT_TORQUE | DB_ITEM_REAR_TORQUE | \
	DB_ITEM_SPEED | DB_ITEM_GPS_ELEVATION | \
	DB_ITEM_HV_CELL_V,
	96,                      // Number of cells in HV battery
	{-200.0, 300.0, 50.0},   // power_kw_range
	{0.0, 16.0, 3.0},        // aux_kw_range
	{-200.0, 500.0, 100.0},  // torque_nm_range
	{-400.0, 800.0, 100.0},  // hv_batt_i_range
	{10.0, 16.0, 2.0},       // lv_batt_v_range
	{3.0, 4.2, 0.2},         // hv_batt_cell_range
	true,                    // 500k CAN
	500,                     // Request timeout (mSec)
	_vw_meb_init,
	_vw_meb_eval,
	_vw_meb_set_req_mask,
	_vw_meb_rx_data,
	_vw_meb_error
};

// ID.Buzz LWB RWD
const vehicle_config_t vehicle_vw_meb104_rwd = {
	"VW MEB104 RWD",
	DB_ITEM_HV_BATT_V | DB_ITEM_HV_BATT_I | DB_ITEM_HV_BATT_MIN_T | DB_ITEM_HV_BATT_MAX_T | \
	DB_ITEM_LV_BATT_V | DB_ITEM_LV_BATT_I | \
	DB_ITEM_AUX_KW | DB_ITEM_REAR_TORQUE | \
	DB_ITEM_SPEED | DB_ITEM_GPS_ELEVATION | \
	DB_ITEM_HV_CELL_V,
	104,                     // Number of cells in HV battery
	{-200.0, 300.0, 50.0},   // power_kw_range
	{0.0, 16.0, 3.0},        // aux_kw_range
	{-200.0, 500.0, 100.0},  // torque_nm_range
	{-400.0, 600.0, 100.0},  // hv_batt_i_range
	{10.0, 16.0, 2.0},       // lv_batt_v_range
	{3.0, 4.2, 0.2},         // hv_batt_cell_range
	true,                    // 500k CAN
	500,                     // Request timeout (mSec)
	_vw_meb_init,
	_vw_meb_eval,
	_vw_meb_set_req_mask,
	_vw_meb_rx_data,
	_vw_meb_error
};

// ID.Buzz LWB AWD
const vehicle_config_t vehicle_vw_meb104_awd = {
	"VW MEB104 AWD",
	DB_ITEM_HV_BATT_V | DB_ITEM_HV_BATT_I | DB_ITEM_HV_BATT_MIN_T | DB_ITEM_HV_BATT_MAX_T | \
	DB_ITEM_LV_BATT_V | DB_ITEM_LV_BATT_I | \
	DB_ITEM_AUX_KW | DB_ITEM_FRONT_TORQUE | DB_ITEM_REAR_TORQUE | \
	DB_ITEM_SPEED | DB_ITEM_GPS_ELEVATION | \
	DB_ITEM_HV_CELL_V,
	104,                     // Number of cells in HV battery
	{-200.0, 300.0, 50.0},   // power_kw_range
	{0.0, 16.0, 3.0},        // aux_kw_range
	{-200.0, 500.0, 100.0},  // torque_nm_range
	{-400.0, 800.0, 100.0},  // hv_batt_i_range
	{10.0, 16.0, 2.0},       // lv_batt_v_range
	{3.0, 4.2, 0.2},         // hv_batt_cell_range
	true,                    // 500k CAN
	500,                     // Request timeout (mSec)
	_vw_meb_init,
	_vw_meb_eval,
	_vw_meb_set_req_mask,
	_vw_meb_rx_data,
	_vw_meb_error
};

// ID.3 Pro RWD
const vehicle_config_t vehicle_vw_meb108_rwd = {
	"VW MEB108 RWD",
	DB_ITEM_HV_BATT_V | DB_ITEM_HV_BATT_I | DB_ITEM_HV_BATT_MIN_T | DB_ITEM_HV_BATT_MAX_T | \
	DB_ITEM_LV_BATT_V | DB_ITEM_LV_BATT_I | \
	DB_ITEM_AUX_KW | DB_ITEM_REAR_TORQUE | \
	DB_ITEM_SPEED | DB_ITEM_GPS_ELEVATION | \
	DB_ITEM_HV_CELL_V,
	108,                     // Number of cells in HV battery
	{-150.0, 250.0, 50.0},   // power_kw_range
	{0.0, 16.0, 3.0},        // aux_kw_range
	{-200.0, 400.0, 100.0},  // torque_nm_range
	{-300.0, 500.0, 100.0},  // hv_batt_i_range
	{10.0, 16.0, 2.0},       // lv_batt_v_range
	{3.0, 4.2, 0.2},         // hv_batt_cell_range
	true,                    // 500k CAN
	500,                     // Request timeout (mSec)
	_vw_meb_init,
	_vw_meb_eval,
	_vw_meb_set_req_mask,
	_vw_meb_rx_data,
	_vw_meb_error
};



//
// Vehicle UDS service CAN request packets (must match list of indicies)
//
//                                                 Req ID      Rsp ID         PCI   SID                                      indexed  index base
static const can_request_t req_12v_batt_info   = {     0x710,      0x77A, 8, {0x03, 0x22, 0x2A, 0xF7, 0x00, 0x00, 0x00, 0x00}, false, 0};
//static const can_request_t req_hv_ptc_current  = {0x17fc007b, 0x17fe007b, 8, {0x03, 0x22, 0x16, 0x20, 0x00, 0x00, 0x00, 0x00}, false, 0}; // Not necessary (in Aux)
static const can_request_t req_gps_info        = {     0x767,      0x7D1, 8, {0x03, 0x22, 0x24, 0x30, 0x00, 0x00, 0x00, 0x00}, false, 0};
static const can_request_t req_aux_power       = {0x17fc0076, 0x17fe0076, 8, {0x03, 0x22, 0x03, 0x64, 0x00, 0x00, 0x00, 0x00}, false, 0};
static const can_request_t req_hv_batt_current = {0x17fc007b, 0x17fe007b, 8, {0x03, 0x22, 0x1E, 0x3D, 0x00, 0x00, 0x00, 0x00}, false, 0};
static const can_request_t req_hv_batt_min_t   = {0x17fc007b, 0x17fe007b, 8, {0x03, 0x22, 0x1E, 0x0F, 0x00, 0x00, 0x00, 0x00}, false, 0};
static const can_request_t req_hv_batt_max_t   = {0x17fc007b, 0x17fe007b, 8, {0x03, 0x22, 0x1E, 0x0E, 0x00, 0x00, 0x00, 0x00}, false, 0};
static const can_request_t req_hv_batt_volt    = {0x17fc007b, 0x17fe007b, 8, {0x03, 0x22, 0x1E, 0x3B, 0x00, 0x00, 0x00, 0x00}, false, 0};
static const can_request_t req_front_torque    = {0x17fc0076, 0x17fe0076, 8, {0x03, 0x22, 0x03, 0x35, 0x00, 0x00, 0x00, 0x00}, false, 0};
static const can_request_t req_rear_torque     = {0x17fc0076, 0x17fe0076, 8, {0x03, 0x22, 0x03, 0x3B, 0x00, 0x00, 0x00, 0x00}, false, 0};
static const can_request_t req_gear_pos        = {0x17fc0076, 0x17fe0076, 8, {0x03, 0x22, 0x21, 0x0E, 0x00, 0x00, 0x00, 0x00}, false, 0};
static const can_request_t req_speed           = {0x18DB33F1, 0x18DAF101, 8, {0x02, 0x01, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00}, false, 0};
static const can_request_t req_hv_cell_volt    = {0x17fc007b, 0x17fe007b, 8, {0x03, 0x22, 0x1E, 0x40, 0x00, 0x00, 0x00, 0x00}, true, 3};  // data[3] is index base value
static const can_request_t req_car_mode        = {0x17fc007b, 0x17fe007b, 8, {0x03, 0x22, 0x74, 0x48, 0x00, 0x00, 0x00, 0x00}, false, 0};

static const can_request_t* req_full_listP[NUM_UDS_REQ_ITEMS] = {
	&req_12v_batt_info,
	&req_gps_info,
	&req_aux_power,
	&req_hv_batt_current,
	&req_hv_batt_min_t,
	&req_hv_batt_max_t,
	&req_hv_batt_volt,
	&req_front_torque,
	&req_rear_torque,
	&req_gear_pos,
	&req_speed,
	&req_hv_cell_volt,
	&req_car_mode
};



//
// Global variables
//
static const char* TAG = "vehicle_vw_meb";

// OBD2 Request management
static bool req_enable = true;
static can_request_t* req_listP[NUM_UDS_REQ_ITEMS];
static bool req_in_process = false;
static bool req_timeout = false;
static bool saw_error = false;
static bool saw_response = false;
static int req_index = 0;
static int num_req_entries = 0;
static int indexed_val_index = 0;
static int num_indexed_vals = 0;
static int64_t car_mode_req_timestamp = 0;
static int64_t gps_req_timestamp = 0;

// Partial data values
static bool in_reverse = false;

// Car Mode to control CAN access after car shuts down but perhaps power to this device continues
// in order not to set off the alarm with some car software packages
static uint8_t car_mode = 0x00;



//
// Vehicle manager functions
//
static void _vw_meb_init(int if_type)
{
	// We don't need to filter the OBD CAN bus because the car's gateway does that for us
	can_en_rsp_filter(false);
}


static void _vw_meb_eval()
{
	bool inc_req_index = false;
	bool proc_req = true;           // Always send request except for special case UDS_CAR_MODE
	static uint8_t req_data[8];
	int64_t cur_timestamp;
	
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
	if (req_enable && !req_in_process && (num_req_entries > 0)) {
		// Handle the special cases where we time-limit particular IDs by checking for a ID-specific
		// timeout as we spin through the active IDs.
		//   1. Slow down UDS_CAR_MODE requests because we have many seconds to detect a mode change
		//      and we don't want to slow down getting displayable datums.
		//   2. Slow down UDS_GPI_INFO requests because the GPS subsystem doesn't like to be accessed
		//      too frequently and elevation doesn't change that quickly.
		if (req_listP[req_index] == req_full_listP[UDS_CAR_MODE]) {
			cur_timestamp = esp_timer_get_time();  // uSec
			if ((cur_timestamp - car_mode_req_timestamp) < CAR_MODE_REQ_USEC) {
				// Skip sending the request for now
				proc_req = false;
			} else {
				// Allow sending the request and reset the timer
				car_mode_req_timestamp = cur_timestamp;
			}
		} else if (req_listP[req_index] == req_full_listP[UDS_GPS_INFO]) {
			cur_timestamp = esp_timer_get_time();  // uSec
			if ((cur_timestamp - gps_req_timestamp) < GPS_REQ_USEC) {
				// Skip sending the request for now
				proc_req = false;
			} else {
				// Allow sending the request and reset the timer
				gps_req_timestamp = cur_timestamp;
			}
		}
		
		if (proc_req) {
			req_in_process = true;
			saw_response = false;
			req_timeout = false;
			
#ifdef DEBUG_DATA
			ESP_LOGI(TAG, "TX: id = %lx, rsp = %lx, len = %d", req_listP[req_index]->req_id, req_listP[req_index]->rsp_id, req_listP[req_index]->req_len);
#endif
			// Local copy of data in case we have to modify it before sending it
			memcpy(req_data, req_listP[req_index]->data, (size_t) req_listP[req_index]->req_len);
			if (req_listP[req_index]->is_indexed) {
				// Set index data byte with current index
				req_data[req_listP[req_index]->index_loc] = req_listP[req_index]->data[req_listP[req_index]->index_loc] + indexed_val_index++;
			}
			
			if (can_tx_packet(req_listP[req_index]->req_id, req_listP[req_index]->rsp_id, req_listP[req_index]->req_len, req_data)) {
				saw_error = false;
			} else {
				saw_error = true;
				ESP_LOGE(TAG, "CAN TX fail - ID = %lx", req_listP[req_index]->req_id);
			}
		}
		
		// Setup for next request
		if (req_listP[req_index]->is_indexed) {
			if (indexed_val_index >= num_indexed_vals) {
				inc_req_index = true;
			}
		} else {
			inc_req_index = true;
		}
		
		if (inc_req_index) {
			req_index += 1;
			if (req_index >= num_req_entries) {
				req_index = 0;
			}
			
			// Handle indexed requests
			if (req_listP[req_index] == req_full_listP[UDS_HV_CELL_VOLT]) {
				indexed_val_index = 0;
				num_indexed_vals = vm_get_indexed_item_count(DB_ITEM_HV_CELL_V);
			}
		}
	}
}


static void _vw_meb_set_req_mask(uint32_t mask)
{
	bool required_req[NUM_UDS_REQ_ITEMS];
	int n = 0;
	
	// Determine what requests are necessary
	required_req[UDS_12V_BATT_INFO] = vm_mask_check(mask, DB_ITEM_LV_BATT_V | DB_ITEM_LV_BATT_I);
	required_req[UDS_GPS_INFO]      = vm_mask_check(mask, DB_ITEM_GPS_ELEVATION);
	required_req[UDS_HV_AUX_PWR]    = vm_mask_check(mask, DB_ITEM_AUX_KW);
	required_req[UDS_HV_BATT_CUR]   = vm_mask_check(mask, DB_ITEM_HV_BATT_I);
	required_req[UDS_HV_BATT_MIN_T] = vm_mask_check(mask, DB_ITEM_HV_BATT_MIN_T);
	required_req[UDS_HV_BATT_MAX_T] = vm_mask_check(mask, DB_ITEM_HV_BATT_MAX_T);
	required_req[UDS_HV_BATT_VOLT]  = vm_mask_check(mask, DB_ITEM_HV_BATT_V | DB_ITEM_AUX_KW);
	required_req[UDS_FRONT_TORQUE]  = vm_mask_check(mask, DB_ITEM_FRONT_TORQUE);
	required_req[UDS_REAR_TORQUE]   = vm_mask_check(mask, DB_ITEM_REAR_TORQUE);
	required_req[UDS_GEAR_POSITION] = vm_mask_check(mask, DB_ITEM_FRONT_TORQUE | DB_ITEM_REAR_TORQUE);
	required_req[UDS_SPEED]         = vm_mask_check(mask, DB_ITEM_SPEED);
	required_req[UDS_HV_CELL_VOLT]  = vm_mask_check(mask, DB_ITEM_HV_CELL_V);
	required_req[UDS_CAR_MODE]      = mask != 0; // Always enabled whenever we want to request data
	
	// Build up our list of requests and reset starting point
	num_req_entries = 0;
	req_index = 0;
	indexed_val_index = 0;
	for (int i=0; i<NUM_UDS_REQ_ITEMS; i++) {
		if (required_req[i]) {
			num_req_entries += 1;
			req_listP[n++] = (can_request_t*) req_full_listP[i];
		}
	}
	if (req_listP[req_index] == req_full_listP[UDS_HV_CELL_VOLT]) {
		num_indexed_vals = vm_get_indexed_item_count(DB_ITEM_HV_CELL_V);
	} else {
		num_indexed_vals = 0;
	}
}


static void _vw_meb_rx_data(uint32_t id, int len, uint8_t* data)
{
	float f;
	int n;
	int16_t i16;
	int32_t i32;
	uint16_t u16;
	
	saw_response = true;
	
#ifdef DEBUG_DATA
	ESP_LOGI(TAG, "RX: id = %lx, len = %d", id, len);
	for (n=0; n<len; n++) {
		printf(" %0X", *(data+n));
	}
	printf("\n");
#endif

	// Try to find the request this response matches
	n = vm_get_resp_index(id, len, data, NUM_UDS_REQ_ITEMS, req_full_listP);
	
	switch (n) {
		case UDS_12V_BATT_INFO:
			if (len == 26) {
				u16 = (data[3] << 8) | data[4];
				f = (float) u16 / 1024.0 + 4.26;
				vm_update_data_item(DB_ITEM_LV_BATT_V, f);
				i32 = (data[5] << 24) | (data[6] << 16) | (data[7] << 8) | data[8];
				f = (float) i32 / 1024.0;
				vm_update_data_item(DB_ITEM_LV_BATT_I, f);
			}
			break;
			
		case UDS_GPS_INFO:
			if (len == 33) {
				u16 = (data[31] << 8) + data[32];
				if ((u16 & 0x8000) == 0x8000) {
					// Work around bug in elevation data greater than 0xEBF.  In my 2024
					// ID.4 I see that data jump from something like 0xEBF to 0xF52B and
					// I calculated through experimentation that it's jumping about 0xE66A
					// for some strage reason.
					u16 = u16 - 0xE66A;
				}
				f = (float) u16 - 501.0;
				vm_update_data_item(DB_ITEM_GPS_ELEVATION, f);
			}
			break;
			
		case UDS_HV_AUX_PWR:
			if (len == 5) {
				i16 = (data[3] << 8) | data[4];
				f = (float) i16 / 10.0;
				vm_update_data_item(DB_ITEM_AUX_KW, f);
			}
			break;
			
		case UDS_HV_BATT_CUR:
			if (len == 8) {
				i32 = (data[3] << 24) | (data[4] << 16) | (data[5] << 8) | data[6];
				f = (i32 - 150000) / 100.0;
				vm_update_data_item(DB_ITEM_HV_BATT_I, f);
			}
			break;
			
		case UDS_HV_BATT_MIN_T:
			if (len == 7) {
				i16 = ((int16_t) ((data[3] << 8) | data[4])) / 64;
				f = (float) i16;
				vm_update_data_item(DB_ITEM_HV_BATT_MIN_T, f);
			}
			break;
			
		case UDS_HV_BATT_MAX_T:
			if (len == 7) {
				i16 = ((int16_t) ((data[3] << 8) | data[4])) / 64;
				f = (float) i16;
				vm_update_data_item(DB_ITEM_HV_BATT_MAX_T, f);
			}
			break;
			
		case UDS_HV_BATT_VOLT:
			if (len == 5) {
				i16 = (data[3] << 8) | data[4];
				f = (float) i16 / 4.0;
				vm_update_data_item(DB_ITEM_HV_BATT_V, f);
			}
			break;
			
		case UDS_FRONT_TORQUE:
			if (len == 5) {
				i16 = (data[3] << 8) | data[4];
				f = (float) i16;
				
				// For MEB the torque value is the actual torque going to the motor
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
			
		case UDS_REAR_TORQUE:
			if (len == 5) {
				i16 = (data[3] << 8) | data[4];
				f = (float) i16;
				if (in_reverse) {
					f = -f;
				}
				vm_update_data_item(DB_ITEM_REAR_TORQUE, f);
			}
			break;
			
		case UDS_GEAR_POSITION:
			if (len == 5) {
				in_reverse = (data[4] == GEAR_REVERSE);
			}
			break;
			
		case UDS_SPEED:
			if (len == 3) {
				f = (float) data[2];
				vm_update_data_item(DB_ITEM_SPEED, f);
			}
			break;
			
		case UDS_HV_CELL_VOLT:
			if (len == 5) {
				u16 = (data[3] << 8) | data[4];
				f = ((float) u16 / 1000.0) + 1.0;
				i16 = data[2] - req_full_listP[UDS_HV_CELL_VOLT]->data[req_full_listP[UDS_HV_CELL_VOLT]->index_loc];
				if (i16 == (vm_get_indexed_item_count(DB_ITEM_HV_CELL_V) - 1)) {
					// Final index value
					vm_update_indexed_data_item(DB_ITEM_HV_CELL_V, (int) i16, f, true);
				} else {
					// Intermediate index value
					vm_update_indexed_data_item(DB_ITEM_HV_CELL_V, (int) i16, f, false);
				}
			}
			break;
			
		case UDS_CAR_MODE:
			if (len == 4) {
				if (data[3] != car_mode) {
					if (req_enable && (car_mode != MODE_STANDBY) && (data[3] == MODE_STANDBY)) {
						ESP_LOGI(TAG, "Car Standby, disabling CAN requests");
						req_enable = false;
					} else if (!req_enable && (car_mode == MODE_STANDBY) && (data[3] != MODE_STANDBY)) {
						ESP_LOGI(TAG, "Car Active, enabling CAN requests");
						req_enable = true;
					}
					car_mode = data[3];
				}
			}
			break;
	}
}


static void _vw_meb_error(int errno)
{
	// We only handle (and expect) timeouts
	if (errno == CAN_ERRNO_TIMEOUT) {
		req_timeout = true;
	}
}
