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
#ifndef DATA_BROKER_H
#define DATA_BROKER_H

#include "esp_system.h"
#include <stdbool.h>
#include <stdint.h>



//
// Global constants
//

// Data Item Masks
//  - All units metric (e.g. °C)
//  - Battery voltage in volts
//  - Battery current negative for discharge, positive for charge
//  - Torque in N-m
//  - Elevation in meters
#define DB_ITEM_HV_BATT_V         0x00000001
#define DB_ITEM_HV_BATT_I         0x00000002
#define DB_ITEM_HV_BATT_MIN_T     0x00000004
#define DB_ITEM_HV_BATT_MAX_T     0x00000008
#define DB_ITEM_LV_BATT_V         0x00000010
#define DB_ITEM_LV_BATT_I         0x00000020
#define DB_ITEM_LV_BATT_T         0x00000040
#define DB_ITEM_AUX_KW            0x00000100
#define DB_ITEM_FRONT_TORQUE      0x00001000
#define DB_ITEM_REAR_TORQUE       0x00002000
#define DB_ITEM_SPEED             0x00010000
#define DB_ITEM_GPS_ELEVATION     0x00100000

// DB_MASK_ITEMS is a power-of-2 indicating size of mask variable
#define DB_MAX_ITEMS              32



//
// Callback handler definition
//
typedef void (*gui_item_value_handler)(float val);


//
// API
//
esp_err_t db_init();
void db_enable_fast_average(bool en);
void db_gui_eval();

// GUI API
void db_register_gui_callback(uint32_t mask, gui_item_value_handler fcn);

// Vehicle Manager API
void db_set_data_item_value(uint32_t mask, float val);

#endif /* DATA_BROKER_H */