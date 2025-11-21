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
#include "can_manager.h"
#include "can_task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gui_task.h"
#include "ps_utilities.h"
#include "vehicle_manager.h"
#include <string.h>


//
// CAN Task variables
//
static const char* TAG = "can_task";

// Task handle
TaskHandle_t task_handle_can;

// Configuration
static main_config_t* configP;



//
// API
//
void can_task()
{
	ESP_LOGI(TAG, "Start task");
	
	// Delay to let GUI task start first
	vTaskDelay(pdMS_TO_TICKS(250));
	
	// Get the system configuration
	if (!ps_get_config(PS_CONFIG_TYPE_MAIN, (void**) &configP)) {
		ESP_LOGE(TAG, "Get configuration failed");
		vTaskDelete(NULL);
	}
	
	// Get the vehicle type
	if (strlen(configP->vehicle_name) == 0) {
		// No vehicle stored (persistent storage freshly initialized), select the first (assume there is at least one)
		strncpy(configP->vehicle_name, vm_get_vehicle_name(0), PS_VEHICLE_NAME_MAX_LEN);
		
		// Save back to persistent storage
		ps_save_config(PS_CONFIG_TYPE_MAIN);
	}
	
	// Attempt to open the selected interface
	if (!vm_init(configP->vehicle_name, configP->connection_index)) {
		ESP_LOGE(TAG, "Vehicle manager init failed - %s, %d", configP->vehicle_name, configP->connection_index);
	}
	
	// Let the GUI know we're up and running
	xTaskNotify(task_handle_gui, GUI_NOTIFY_VEHICLE_INIT, eSetBits);
	
	while (1) {
		vTaskDelay(pdMS_TO_TICKS(CAN_TASK_EVAL_MSEC));
		
		if (can_connected()) {
			vm_eval();
		}
	}
}
