/*
 * Application entry-point for ev_info_display.  Initializes the system and starts the tasks
 * that implement its functionality.
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
 */
#include "Buzzer.h"
#include "data_broker.h"
#include "esp_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "can_task.h"
#include "gui_task.h"
#include "I2C_Driver.h"
#include "TCA9554PWR.h"
#include "ps_utilities.h"
 

//
// Variables
//
static const char* TAG = "main";


//
// API
//
void app_main(void)
{
	ESP_LOGI(TAG, "ev_info_display starting");
	
	// Initialize persistent storage so everyone can get their configuration
	if (!ps_init()) {
		ESP_LOGE(TAG, "Persistent Storage initialization failed");
		while (1) {vTaskDelay(pdMS_TO_TICKS(100));}
	}
	
	// Initialize shared resources
	ESP_ERROR_CHECK(I2C_Init());
	ESP_ERROR_CHECK(EXIO_Init());
	ESP_ERROR_CHECK(db_init());
	
	// Let them know we're alive
	Buzzer_On();
	vTaskDelay(pdMS_TO_TICKS(100));
	Buzzer_Off();
	
	// Start tasks
    //  Core 0 : PRO
    //  Core 1 : APP
    xTaskCreatePinnedToCore(&can_task,   "can_task",   3072, NULL, 2, &task_handle_can,   0);
    xTaskCreatePinnedToCore(&gui_task,   "gui_task",   3072, NULL, 2, &task_handle_gui,   1);
}