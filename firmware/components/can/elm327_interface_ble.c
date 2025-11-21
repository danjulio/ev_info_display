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
#include "ble_utilities.h"
#include "can_driver_elm327.h"
#include "elm327_interface_ble.h"
#include "esp_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <string.h>


//
// Local constants
//

// Uncomment to debug TX/RX data
//#define DEBUG_SHOW_DATA

// Connection state
#define DRIVER_STATE_NO_BLE        0
#define DRIVER_STATE_BLE_SCAN      1
#define DRIVER_STATE_BLE_SCAN_DONE 2
#define DRIVER_STATE_CONNECTED     3


//
// Functions for can_driver_elm327
//
static bool elm327_interface_ble_init(int debug);
static bool elm327_interface_ble_tx_line(char* s);


const elm327_if_driver_t elm327_interface_driver_ble =
{
	"ELM327 Interface BLE",
	&elm327_interface_ble_init,
	&elm327_interface_ble_tx_line
};


//
// Global variables
//
static const char* TAG = "elm327_interface_ble";

static TaskHandle_t task_handle_elm327_interface_ble;

// State
static int driver_state = DRIVER_STATE_NO_BLE;

// TX Buffer
static bool tx_buffer_valid = false;
static char tx_buffer[CAN_DRIVER_MAX_ELM327_STR_LEN+2];  // Room for carriage return and null
static SemaphoreHandle_t tx_mutex;

// RX Buffer
static char rx_buffer[CAN_DRIVER_MAX_ELM327_STR_LEN+1];



//
// Forward declarations for internal functions
//
static void _elm327_interface_ble_task();
static void _elm327_interface_ble_scan_complete();
static void _elm327_interface_ble_rx_cb(int len, uint8_t* data);


//
// CAN driver functions
//
static bool elm327_interface_ble_init()
{
	if (!ble_init(&_elm327_interface_ble_scan_complete, &_elm327_interface_ble_rx_cb)) {
		ESP_LOGE(TAG, "Could not initialize BLE");
		return false;
	}
	
	// Create the tx mutex
	tx_mutex = xSemaphoreCreateMutex();
	if (tx_mutex == NULL) {
		ESP_LOGE(TAG, "Could not create tx_mutex");
		return false;
	}
	
	// Start our task on the protocol CPU
	xTaskCreatePinnedToCore(&_elm327_interface_ble_task, "elm327_interface_ble_task", 4096, NULL, 2, &task_handle_elm327_interface_ble, 0);
	
	return true;
}


static bool elm327_interface_ble_tx_line(char* s)
{
	bool ret = false;
	int i;
	
	xSemaphoreTake(tx_mutex, portMAX_DELAY);
	
	if (driver_state == DRIVER_STATE_CONNECTED) {
		strncpy(tx_buffer, s, CAN_DRIVER_MAX_ELM327_STR_LEN);
		i = strlen(tx_buffer);
		tx_buffer[i++] = 0x0D;  // Add Carriage Return
		tx_buffer[i] = 0;       // Null terminate
		tx_buffer_valid = true;
		ret = true;
	}
	
	xSemaphoreGive(tx_mutex);
	
	return ret;
}



//
// Internal functions
//
static void _elm327_interface_ble_task()
{
	ESP_LOGI(TAG, "Start task");
	
	while (1) {
		switch (driver_state) {
			case DRIVER_STATE_NO_BLE:
				// Idle waiting for BLE stack initialization
				vTaskDelay(pdMS_TO_TICKS(50));
				if (ble_is_enabled()) {
					// Start a scan and connect procedure
					if (ble_start_scan()) {
						ESP_LOGI(TAG, "Scanning for device");
						driver_state = DRIVER_STATE_BLE_SCAN;
					} else {
						vTaskDelay(pdMS_TO_TICKS(500));
					}
				}
				break;
			
			case DRIVER_STATE_BLE_SCAN:
				// Spin waiting for scan complete to take us out of this state
				vTaskDelay(pdMS_TO_TICKS(50));
				break;
			
			case DRIVER_STATE_BLE_SCAN_DONE:
				if (ble_is_connected()) {
					driver_state = DRIVER_STATE_CONNECTED;
					can_driver_elm327_set_connected(true);
				} else {
					// Start another scan
					driver_state = DRIVER_STATE_NO_BLE;
				}
				break;
			
			case DRIVER_STATE_CONNECTED:
			// Look for strings to send
				xSemaphoreTake(tx_mutex, portMAX_DELAY);
				if (tx_buffer_valid) {
					tx_buffer_valid = false;
#ifdef DEBUG_SHOW_DATA
					ESP_LOGI(TAG, "TX: %s", tx_buffer);
#endif
					if (!ble_tx_data(strlen(tx_buffer), tx_buffer)) {
						ESP_LOGE(TAG, "BLE TX failed");
						can_driver_elm327_tx_failed();
					}
				}
				xSemaphoreGive(tx_mutex);
				
				if (!ble_is_connected()) {
					driver_state = DRIVER_STATE_NO_BLE;
					can_driver_elm327_set_connected(false);
				}
				
				vTaskDelay(pdMS_TO_TICKS(10));
				break;
			
			default:
				break;
		}
	}
}


static void _elm327_interface_ble_scan_complete(int debug)
{
	ESP_LOGI(TAG, "Scan complete - %d", debug);
	driver_state = DRIVER_STATE_BLE_SCAN_DONE;
}


static void _elm327_interface_ble_rx_cb(int len, uint8_t* data)
{
	size_t rx_len;
	
#ifdef DEBUG_SHOW_DATA
	printf("%s RX: ", TAG);
	for (int i=0; i<len; i++) {
		printf("0x%2x ", data[i]);
	}
	printf("\n");
#endif
	
	rx_len = (len > CAN_DRIVER_MAX_ELM327_STR_LEN) ? CAN_DRIVER_MAX_ELM327_STR_LEN : len;
	strncpy(rx_buffer, (const char*) data, rx_len);
	rx_buffer[rx_len] = 0;
	can_driver_elm327_rx_data(rx_buffer);
}
