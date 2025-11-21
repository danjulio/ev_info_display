/*
 * TWAI CAN driver
 *
 * Provide a simple interface to the Espressif IDF TWAI peripheral driver.
 * Designed to be used by can_manager.
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
#ifndef CAN_DRIVER_TWAI_H
#define CAN_DRIVER_TWAI_H

#include <can_manager.h>


//
// Global constants
//
#define TWAI_PIN_TX 43
#define TWAI_PIN_RX 44



//
// Externs for driver definition
//
extern const can_if_driver_t can_driver_twai;


#endif /* CAN_DRIVER_TWAI_H */
