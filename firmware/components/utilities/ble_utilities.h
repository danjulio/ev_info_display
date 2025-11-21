/*
 * BLE related utilities
 *
 * Contains functions to initialize, configure and query the BLE interface.
 *
 * Code design inspired by https://gitlab.com/janoskut/esp32-obd2-meter
 *
 * Copyright 2025 Dan Julio
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */
#ifndef BLE_UTILITIES_H
#define BLE_UTILITIES_H

#include <stdbool.h>
#include <stdint.h>



//
// Typedef for RX callback
//
typedef void (*ble_scan_complete_fcn)(int debug);          // Scan done callback (returns internal diag info)
typedef void (*ble_rx_data_fcn)(int len, uint8_t* data);   // Receive data callback



//
// BLE Utilities API
//
bool ble_init(ble_scan_complete_fcn scan_fcn, ble_rx_data_fcn rx_fcn);
bool ble_start_scan();
bool ble_is_enabled();
bool ble_is_connected();
bool ble_tx_data(int len, char* data);

#endif /* BLE_UTILITIES_H */
