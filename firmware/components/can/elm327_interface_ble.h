/*
 * ELM327 BLE driver BLE interface
 *
 * Implement the stream interface for the ELM327 driver over BLE.  Manage BLE
 * connection.  Designed to be used by can_driver_elm327.
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
#ifndef ELM327_INTERFACE_BLE_H
#define ELM327_INTERFACE_BLE_H

#include <can_driver_elm327.h>
#include <stdbool.h>
#include <stdint.h>



//
// Externs for interface defined in this module
//
extern const elm327_if_driver_t elm327_interface_driver_ble;

#endif /* ELM327_INTERFACE_BLE_H */
