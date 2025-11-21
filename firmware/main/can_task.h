/*
 * CAN Task
 *
 * Manage CAN bus communication through an OBD2 port for selected vehicles
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
#ifndef CAN_TASK_H
#define CAN_TASK_H

#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"



//
// CAN Task Constants
//
#define CAN_TASK_EVAL_MSEC  10



//
// CAN Task externally accessible variables
//
extern TaskHandle_t task_handle_can;



//
// API
//
void can_task();

#endif /* CAN_TASK_H */