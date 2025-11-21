/*
 * ELM327 CAN driver
 *
 * Provide a simple interface to a stream-based sub-driver to communicate with an
 * ELM327-based CAN bus interface.  Designed to be used by can_manager.
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
#ifndef CAN_DRIVER_ELM327_H
#define CAN_DRIVER_ELM327_H

#include <can_manager.h>
#include <stdbool.h>
#include <stdint.h>


//
// Global constants
//

// List of all implemented interfaces
#define CAN_DRIVER_ELM327_WIFI     0
#define CAN_DRIVER_ELM327_BLE      1

#define CAN_DRIVER_ELM327_NUM_IF   2

// Max ELM327 controller command or response string length
#define CAN_DRIVER_MAX_ELM327_STR_LEN  80



//
// Interface driver functions
//
typedef bool (*elm327_if_init)();
typedef bool (*elm327_if_tx_line)(char* s);



//
// Global Data structures
//
typedef struct {
	char* name;
	elm327_if_init fcn_init;
	elm327_if_tx_line fcn_tx_line;
} elm327_if_driver_t;


//
// Externs for driver definition
//
extern const can_if_driver_t can_driver_elm327;


//
// API
//

// For ELM327 interface driver
void can_driver_elm327_set_connected(bool connected);
void can_driver_elm327_tx_failed();
void can_driver_elm327_rx_data(char* s);

#endif /* CAN_DRIVER_ELM327_H */
