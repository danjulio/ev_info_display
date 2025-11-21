/*
 * CAN Manager
 *
 * Manager for various OBD2 CAN interfaces.  Provides a unified ISO-TP compatible
 * interface to initialize the selected OBD2 CAN interface driver, send request packets
 * and receive complete responses.  Exists between the Vehicle Manager and
 * OBD2 interface.  Implements basic [simplified] ISO-TP data management.
 *
 * Note: Only supports one request at a time
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
#include "can_driver_twai.h"
#include "can_driver_elm327.h"
#include "esp_system.h"
#include "esp_log.h"
#include "vehicle_manager.h"



//
// Local constants
//

// List of all implemented interfaces
#define DRIVER_TWAI   0
#define DRIVER_ELM327 1

#define NUM_DRIVERS   2

static const can_if_driver_t* interface_listP[] = {
	&can_driver_twai,
	&can_driver_elm327
};



//
// Global variables
//
static const char* TAG = "can_manager";

static can_if_driver_t* driverP = NULL;
static uint32_t cur_req_id = 0;
static uint32_t cur_rsp_id = 0;
static uint8_t data_buf[4096];

// Data for flow-control message
static const uint8_t flow_control_data[] = {0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};



//
// API
//
int can_get_num_interfaces()
{
	return CAN_MANAGER_NUM_IF;
}


const char* can_get_interface_name(int n)
{
	switch (n) {
		case CAN_MANAGER_IF_TWAI:
			return "HW CAN";
			break;
		case CAN_MANAGER_IF_WIFI:
			return "ELM327 WIFI";
			break;
		case CAN_MANAGER_IF_BLE:
			return "ELM327 BLE";
			break;
		default:
			return NULL;
	}
}


bool can_init(int if_type, int req_timeout, bool can_is_500k)
{
	bool ret;
	
	switch (if_type) {
		case CAN_MANAGER_IF_TWAI:
			driverP = (can_if_driver_t*) interface_listP[DRIVER_TWAI];
			ret = driverP->fcn_init(0, req_timeout, can_is_500k);
			break;
		
		case CAN_MANAGER_IF_WIFI:
			driverP = (can_if_driver_t*) interface_listP[DRIVER_ELM327];
			ret = driverP->fcn_init(CAN_DRIVER_ELM327_WIFI, req_timeout, can_is_500k);
			break;
			
		case CAN_MANAGER_IF_BLE:
			driverP = (can_if_driver_t*) interface_listP[DRIVER_ELM327];
			ret = driverP->fcn_init(CAN_DRIVER_ELM327_BLE, req_timeout, can_is_500k);
			break;
		
		default:
			ret = false;
	}
	
	return ret;
}


bool can_connected()
{
	if (driverP != NULL) {
		return driverP->fcn_is_connected();
	}
	
	return false;
}


bool can_tx_packet(uint32_t req_id, uint32_t rsp_id, int len, uint8_t* data)
{
	if (driverP != NULL) {
		// Store the request ID and response ID for later comparison with received data
		cur_req_id = req_id;
		cur_rsp_id = rsp_id;
		
		// Attempt to send the packet
		return driverP->fcn_tx_packet(req_id, rsp_id, len, data);
	}
	
	return false;
}


void can_en_rsp_filter(bool en)
{
	if (driverP != NULL) {
		driverP->fcn_en_rsp_filter(en);
	}
}


// Note this may be called from within an ISR which means the Vehicle Manager vm_rx_data()
// will also be called from within an ISR
void can_rx_packet(uint32_t rsp_id, int len, uint8_t* data)
{
	bool is_singleframe = false;
	bool is_firstframe = false;
	bool is_consecutiveframe = false;
	int rx_data_index;
	static int num_rx_bytes;
	static int data_index;
	static uint8_t seq_num;
	
	if (rsp_id == cur_rsp_id) {
		if (len > 0) {
			switch (data[0] & 0xF0) {
				case 0x00:
					// Single frame
					is_singleframe = true;
					num_rx_bytes = data[0] & 0x0F;
					rx_data_index = 1;               // Index of first data byte
					data_index = 0;                  // Start collecting data
					seq_num = 0xFF;                  // Should see no subsequent frame packets so ignore any we see
					break;
				case 0x10:
					// First frame of a multiframe response
					if (len > 1) {
						is_firstframe = true;
						num_rx_bytes = ((data[0] & 0x0F) << 8) | data[1];
						rx_data_index = 2;
						data_index = 0;
						seq_num = 0;
					} else {
						// Invalid packet so set an invalid sequence number for force ignoring subsequent data
						seq_num = 0xFF;
					}
					break;
				case 0x20:
					// Subsequent frame of a multiframe response
					is_consecutiveframe = true;
					rx_data_index = 1;
					break;
			}
		}
		
		// Copy data to our local buffer
		if (is_singleframe || is_firstframe || (is_consecutiveframe && ((data[0] & 0x0F) == seq_num))) {
			while ((rx_data_index < len) && (data_index < num_rx_bytes)) {
				data_buf[data_index++] = data[rx_data_index++];
			}
			seq_num = (seq_num + 1) & 0x0F;
			
			if (data_index == num_rx_bytes) {
				// Received a complete response, stop the driver's timeout timer
				driverP->fcn_response_complete();
				
				// And send it to the vehicle
				vm_rx_data(rsp_id, num_rx_bytes, data_buf);
			}
		}
		
		// Send flow control packet if necessary
		if (is_firstframe && (cur_req_id != 0)) {
			(void) driverP->fcn_tx_fc_packet(cur_req_id, 8, (uint8_t*) flow_control_data);
		}
	}
}


void can_if_error(int errno)
{
	vm_note_error(errno);
}
