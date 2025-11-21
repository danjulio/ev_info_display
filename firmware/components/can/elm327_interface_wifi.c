/*
 * ELM327 CAN driver Wifi interface
 *
 * Implement the stream interface for the ELM327 driver over Wifi.  Manage Wifi
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
#include "can_driver_elm327.h"
#include "elm327_interface_wifi.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "ps_utilities.h"
#include "wifi_utilities.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>            // struct addrinfo
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>



//
// Local constants
//

// Uncomment to debug TX/RX data
//#define DEBUG_SHOW_DATA

// Connection state
#define DRIVER_STATE_NO_WIFI    0
#define DRIVER_STATE_WIFI       1
#define DRIVER_STATE_CONNECTED  2



//
// Functions for can_driver_elm327
//
static bool elm327_interface_wifi_init();
static bool elm327_interface_wifi_tx_line(char* s);


const elm327_if_driver_t elm327_interface_driver_wifi =
{
	"ELM327 Interface Wifi",
	&elm327_interface_wifi_init,
	&elm327_interface_wifi_tx_line
};



//
// Global variables
//
static const char* TAG = "elm327_interface_wifi";

// Local initialization task
static TaskHandle_t task_handle_elm327_interface_wifi;

// Wifi configuration
static net_config_t* configP;

// State
static int driver_state = DRIVER_STATE_NO_WIFI;

// TX Buffer
static bool tx_buffer_valid = false;
static char tx_buffer[CAN_DRIVER_MAX_ELM327_STR_LEN+2];  // Room for carriage return and null
static SemaphoreHandle_t tx_mutex;



//
//  Forward declarations for internal functions
//
static void _elm327_interface_wifi_task();



//
// CAN driver functions
//
static bool elm327_interface_wifi_init()
{
	// Attempt to initialize Wifi
	if (!wifi_init()) {
		ESP_LOGE(TAG, "Could not initialize Wifi");
		return false;
	}
	
	// Create the tx mutex
	tx_mutex = xSemaphoreCreateMutex();
	if (tx_mutex == NULL) {
		ESP_LOGE(TAG, "Could not create tx_mutex");
		return false;
	}
	
	// Get our wifi configuration
	if (!ps_get_config(PS_CONFIG_TYPE_NET, (void**) &configP)) {
		ESP_LOGE(TAG, "Get configuration failed");
		return false;
	}
	
	// Start our task on the protocol CPU
	xTaskCreatePinnedToCore(&_elm327_interface_wifi_task, "elm327_interface_wifi_task", 4096, NULL, 2, &task_handle_elm327_interface_wifi, 0);
	
	driver_state = DRIVER_STATE_NO_WIFI;
	
	return true;
}



static bool elm327_interface_wifi_tx_line(char* s)
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
static void _elm327_interface_wifi_task()
{
	char err_buf[80];
	char rx_buffer[CAN_DRIVER_MAX_ELM327_STR_LEN+1];
	char host_ip[32];
	int addr_family = 0;
	int err;
	int ip_protocol = 0;
	int len;
	int sock;
	struct sockaddr_in dest_addr;
	
	ESP_LOGI(TAG, "Start task");
	
	while (1) {
		if (driver_state == DRIVER_STATE_NO_WIFI) {
			// Just idle waiting for a Wifi connection
			vTaskDelay(pdMS_TO_TICKS(50));
			
			if (wifi_is_connected()) {
				ESP_LOGI(TAG, "Wifi connected");
				driver_state = DRIVER_STATE_WIFI;
			}
		} else {
			// Get the IP address of our DNS server (this should be the Wifi OBD dongle)
			wifi_get_ipv4_gw_string(host_ip);
			
			// Create the socket
			inet_pton(AF_INET, host_ip, &dest_addr.sin_addr);
        	dest_addr.sin_family = AF_INET;
        	dest_addr.sin_port = htons(configP->remote_port);
        	addr_family = AF_INET;
        	ip_protocol = IPPROTO_IP;
        	sock = socket(addr_family, SOCK_STREAM, ip_protocol);
        	if (sock < 0) {
        		if (wifi_is_connected()) {
	        		// Fatal error
 	       			ESP_LOGE(TAG, "Unable to create socket: errno %d - %s", errno, esp_err_to_name_r(errno, err_buf, sizeof(err_buf)));
					break;
				} else {
					driver_state = DRIVER_STATE_NO_WIFI;
				}
        	} else {        	
				// Attempt to connect
				err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
				if (err != 0) {
					ESP_LOGE(TAG, "Socket unable to connect: errno %d - %s", errno, esp_err_to_name_r(errno, err_buf, sizeof(err_buf)));
					vTaskDelay(pdMS_TO_TICKS(500));
				} else {
					ESP_LOGI(TAG, "Socket connected");
					driver_state = DRIVER_STATE_CONNECTED;
					can_driver_elm327_set_connected(true);
					
					while (1) {
						// Look for strings to send
						xSemaphoreTake(tx_mutex, portMAX_DELAY);
						if (tx_buffer_valid) {
							tx_buffer_valid = false;
#ifdef DEBUG_SHOW_DATA
							ESP_LOGI(TAG, "TX: %s", tx_buffer);
#endif
							err = send(sock, tx_buffer, strlen(tx_buffer), 0);
							if (err < 0) {
								can_driver_elm327_tx_failed();
								ESP_LOGI(TAG, "send failed: errno: %d - %s - Socket disconnected", errno, esp_err_to_name_r(errno, err_buf, sizeof(err_buf)));
								break;
							}
						}
						xSemaphoreGive(tx_mutex);
						
						// Look for incoming strings
						len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, MSG_DONTWAIT);
						if (len < 0) {
							if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
								// Nothing there to receive, so just wait before calling recv again
								vTaskDelay(pdMS_TO_TICKS(10));
							} else {
								ESP_LOGI(TAG, "recv failed: errno: %d - %s - Socket disconnected", errno, esp_err_to_name_r(errno, err_buf, sizeof(err_buf)));
								break;
							}
						} else if (len > 0) {
#ifdef DEBUG_SHOW_DATA
							printf("%s RX: ", TAG);
							for (int i=0; i<len; i++) {
								printf("0x%2x ", rx_buffer[i]);
							}
							printf("\n");
#endif
							// Process received data
							rx_buffer[len] = 0;  // Null-terminate string
							can_driver_elm327_rx_data(rx_buffer);
						}
					}
					
					// Clean up
					if (wifi_is_connected()) {
	        			driver_state = DRIVER_STATE_WIFI;
					} else {
						driver_state = DRIVER_STATE_NO_WIFI;
					}
					
					can_driver_elm327_set_connected(false);
				
					if (sock != -1) {
						ESP_LOGE(TAG, "Shutting down socket and restarting...");
						shutdown(sock, 0);
						close(sock);
					}
					
					vTaskDelay(pdMS_TO_TICKS(500));
				}
			}
		}
	}
}
