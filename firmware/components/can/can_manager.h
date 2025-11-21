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
#ifndef CAN_MANAGER_H
#define CAN_MANAGER_H

#include <stdbool.h>
#include <stdint.h>


//
// Constants
//

// CAN Interface type
#define CAN_MANAGER_IF_TWAI 0
#define CAN_MANAGER_IF_WIFI 1
#define CAN_MANAGER_IF_BLE  2

#define CAN_MANAGER_NUM_IF  3

// CAN RX Error codes
#define CAN_ERRNO_NONE      0
#define CAN_ERRNO_TIMEOUT   1


//
// Interface driver functions
//
typedef bool (*can_if_init)(int if_type, int req_timeout, bool can_is_500k);
typedef bool (*can_if_connected)();
typedef bool (*can_if_tx_packet)(uint32_t req_id, uint32_t rsp_id, int len, uint8_t* data);
typedef bool (*can_if_tx_fc_packet)(uint32_t req_id, int len, uint8_t* data);  // May be called from within an ISR
typedef void (*can_if_en_rsp_filter)(bool en);
typedef void (*can_if_response_complete)();



//
// Global Data structures
//
typedef struct {
	char* name;
	can_if_init fcn_init;
	can_if_connected fcn_is_connected;
	can_if_tx_packet fcn_tx_packet;
	can_if_tx_fc_packet fcn_tx_fc_packet;
	can_if_en_rsp_filter fcn_en_rsp_filter;
	can_if_response_complete fcn_response_complete;
} can_if_driver_t;



//
// API
//
// For GUI use
int can_get_num_interfaces();
const char* can_get_interface_name(int n);

// For vehicle implementations
bool can_init(int if_type, int req_timeout, bool can_is_500k);
bool can_connected();
bool can_tx_packet(uint32_t req_id, uint32_t rsp_id, int len, uint8_t* data);
void can_en_rsp_filter(bool en);

// For OBD2 CAN interface drivers
void can_rx_packet(uint32_t rsp_id, int len, uint8_t* data);
void can_if_error(int errno);
#endif /* CAN_MANAGER_H */
