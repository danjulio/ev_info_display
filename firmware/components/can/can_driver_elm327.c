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
#include "can_driver_elm327.h"
#include "elm327_interface_ble.h"
#include "elm327_interface_wifi.h"
#include "esp_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>



//
// Local constants
//

// Uncomment to debug TX/RX data
//#define DEBUG_SHOW_DATA

// Uncomment to debug initialization
//#define DEBUG_SHOW_INIT

// Operational state
#define OP_ST_DISCONNECTED  0
#define OP_ST_INIT_ELM327   1
#define OP_ST_CONNECTED     2

// Transmit string state 
#define TX_ST_IDLE          0
#define TX_ST_AT_CMD        1
#define TX_ST_REQ_PKT       2
#define TX_ST_TIMEOUT       3
#define TX_ST_ERROR         4

// Previous packet size (to update ELM327 protocol on header size differences)
#define HEADER_SIZE_UNDEF   0
#define HEADER_SIZE_11      1
#define HEADER_SIZE_29      2

// RX circular data buffer
#define RX_BUFF_LEN         1024

// Maximum length of ELM327 version string (numeric component - e.g. "2.4"")
// Room for "MM.mm" + Null
#define MAX_ELM327_VER_LEN  6



// Functions for CAN manager
static bool _can_driver_elm327_init(int if_type, int req_timeout, bool can_is_500k);
static bool _can_driver_elm327_connected();
static bool _can_driver_elm327_tx_packet(uint32_t req_id, uint32_t rsp_id, int len, uint8_t* data);
static bool _can_driver_elm327_tx_fc_packet(uint32_t req_id, int len, uint8_t* data);
static void _can_driver_elm327_en_rsp_filter(bool en);
static void _can_driver_elm327_response_complete();


//
//  Forward declarations
//
static void _can_driver_elm327_task();
static void _can_driver_elm327_process_rx_buf();
static bool _can_driver_elm327_tx_string(int pkt_state, char* s);
static char _can_driver_elm327_nibble_2_ascii(uint8_t nibble);
static uint8_t _can_driver_elm327_ascii_to_nibble(char c);
static bool _can_driver_elm327_is_hex_char(char c);
static void _can_driver_elm327_proc_version_info(char c, bool init);


//
// Driver definition
//
const can_if_driver_t can_driver_elm327 =
{
	"CAN ELM327 Driver",
	_can_driver_elm327_init,
	_can_driver_elm327_connected,
	_can_driver_elm327_tx_packet,
	_can_driver_elm327_tx_fc_packet,
	_can_driver_elm327_en_rsp_filter,
	_can_driver_elm327_response_complete
};


//
// Global variables
//
static const char* TAG = "can_driver_elm327";

// Local initialization task
static TaskHandle_t task_handle_elm327_driver;

// Supported interface drivers
static const elm327_if_driver_t* interface_listP[] = {
	&elm327_interface_driver_wifi,
	&elm327_interface_driver_ble
};

// Selected interface driver
static const elm327_if_driver_t* driverP = NULL;

// State
static bool can_500k;
static int timeout_msec;
static int op_state = OP_ST_DISCONNECTED;
static int tx_state = TX_ST_IDLE;
static int prev_header_size = HEADER_SIZE_UNDEF;
static uint32_t prev_req_id = 0;
static uint32_t prev_rsp_id = 0;

// RX Buffer
static char *rx_buf;
static int rx_buf_push_index = 0;
static int rx_buf_pop_index = 0;

// ELM325 adapter information for hacks around crappy and buggy implementations
static char elm327_version_string[MAX_ELM327_VER_LEN];
static bool elm327_is_v15 = false;


// ELM327 IF Initialization sequence
static char* elm327_init_cmd[] =
{
	"ATZ",			// Reset the ELM327 controller
	"ATE0",			// Disable echoing sent data bytes (and commands)
	"ATCAF0",		// Turn off auto formatting so we specify and receive all the data bytes
	"ATCFC1",		// Turn on flow control (for optimization, we'll ignore can_manager's FC packets)
	"ATM0",			// Disable saving protocol changes to memory
	"ATL0",			// Disable sending <LF> after <CR>
	"ATH0",			// Disable header ID in responses
	"ATS1",			// Enable spaces between data bytes (necessary for our parser)
	"ATST7D",		// Set 500 mSec timeout
	"ATFCSH710",	// Set a dummy flow control message for now (so subsequent ATFCSM1 command will succeed)
	"ATFCSD300000",	// Set flow control response bytes
	"ATFCSM1",		// Enable custom flow control response
};
#define NUM_ELM327_INIT_CMDS 12



//
// CAN manager functions
//
static bool _can_driver_elm327_init(int if_type, int req_timeout, bool can_is_500k)
{
	bool success = true;
	
	timeout_msec = req_timeout * 10;   // Accomodate latency in connection + ELM327 controller
	can_500k = can_is_500k;
	
	// Initialize the interface
	switch (if_type) {
		case CAN_DRIVER_ELM327_WIFI:
			driverP = interface_listP[CAN_DRIVER_ELM327_WIFI];
			success = driverP->fcn_init();
			break;
		
		case CAN_DRIVER_ELM327_BLE:
			driverP = interface_listP[CAN_DRIVER_ELM327_BLE];
			success = driverP->fcn_init();
			break;
		
		default:
			success = false;
	}
	
	// Create our receive circular buffer
	rx_buf = malloc(RX_BUFF_LEN);
	if (rx_buf == NULL) {
		ESP_LOGE(TAG, "Could not malloc rx_buf");
		success = false;
	}
	
	// Start our task on the protocol CPU
	if (success) {
		xTaskCreatePinnedToCore(&_can_driver_elm327_task, "can_driver_elm327_task", 3072, NULL, 3, &task_handle_elm327_driver, 0);
	}
	
	return success;
}


static bool _can_driver_elm327_connected()
{
	return (op_state == OP_ST_CONNECTED);
}


// This function is blocking and depends on asynchronous tasks running in the interface
// driver to return status for the various commands it sends to the ELM327 controller.
static bool _can_driver_elm327_tx_packet(uint32_t req_id, uint32_t rsp_id, int len, uint8_t* data)
{
	char tx_str[32];  // Large enough for AT command "ATFCSHnnnnnnnn" or 8-bytes of data - "00 00 00 00 00 00 00 00"
	char* txP;
	int cur_header_size;
	uint8_t* dP;
	
	// Safety...
	if ((driverP == NULL) || (op_state != OP_ST_CONNECTED)) {
		return false;
	}
	
#ifdef DEBUG_SHOW_DATA
	ESP_LOGI(TAG, "TX req 0x%lx, rsp 0x%lx", req_id, rsp_id);
#endif
	
	// Set the appropriate protocol if necessary (previous packet had a different size id)
	cur_header_size = (req_id > 0x7FF) ? HEADER_SIZE_29 : HEADER_SIZE_11;
	if ((prev_header_size == HEADER_SIZE_UNDEF) || (prev_header_size != cur_header_size)) {
		prev_header_size = cur_header_size;
		
		if (cur_header_size == HEADER_SIZE_11) {
			if (can_500k) {
				if (!_can_driver_elm327_tx_string(TX_ST_AT_CMD, "ATTP6")) return false;
			} else {
				if (!_can_driver_elm327_tx_string(TX_ST_AT_CMD, "ATTP8")) return false;
			}
		} else {
			if (can_500k) {
				if (!_can_driver_elm327_tx_string(TX_ST_AT_CMD, "ATTP7")) return false;
			} else {
				if (!_can_driver_elm327_tx_string(TX_ST_AT_CMD, "ATTP9")) return false;
			}
		}
	}
	
	if (req_id != prev_req_id) {
		// Set the request header
		if (elm327_is_v15) {
			// Work around a bug where we can only send 24-bits to ATSH so we also use ATCP
			// for the upper 8-bits
			if (cur_header_size == HEADER_SIZE_29) {
				sprintf(tx_str, "ATCP%lx", (req_id >> 24));
				if (!_can_driver_elm327_tx_string(TX_ST_AT_CMD, tx_str)) return false;
			}
			sprintf(tx_str, "ATSH%lx", req_id & 0xFFFFFF);
			if (!_can_driver_elm327_tx_string(TX_ST_AT_CMD, tx_str)) return false;
		} else {
			sprintf(tx_str, "ATSH%lx", req_id);
			if (!_can_driver_elm327_tx_string(TX_ST_AT_CMD, tx_str)) return false;
		}
		
		// Set the custom flow control header to be the same as the request header
		sprintf(tx_str, "ATFCSH%lx", req_id);
		if (!_can_driver_elm327_tx_string(TX_ST_AT_CMD, tx_str)) return false;
		
		prev_req_id = req_id;
	}
	
	if (rsp_id != prev_rsp_id) {
		// Set the expected response header	
		sprintf(tx_str, "ATCRA%lx", rsp_id);
		if (!_can_driver_elm327_tx_string(TX_ST_AT_CMD, tx_str)) return false;
		
		prev_rsp_id = rsp_id;
	}
	
	if (elm327_is_v15) {
		// Get rid of trailing zeros (because some cheap Chinese OBD clones fail with them)
		for (cur_header_size=len-1; cur_header_size>= 0; cur_header_size--) {
			if (data[cur_header_size] != 0) break;
		}
		len = cur_header_size + 1;
	}
	
	// Send the data as a string
	txP = tx_str;
	dP = data;
	while (len--) {
		*txP++ = _can_driver_elm327_nibble_2_ascii(*dP >> 4);
		*txP++ = _can_driver_elm327_nibble_2_ascii(*dP++ & 0x0F);
	}
	*txP = 0;
	if (!_can_driver_elm327_tx_string(TX_ST_REQ_PKT, tx_str)) return false;
	
	return true;
}


static bool _can_driver_elm327_tx_fc_packet(uint32_t req_id, int len, uint8_t* data)
{
	// This driver skips flow control packets since it configures the ELM327 interface to
	// deal with those
	return true;
}


static void _can_driver_elm327_en_rsp_filter(bool en)
{
	// This driver ignores filtering since it always configures the ELM327 interface
	// with the specific response it is looking for
}


static void _can_driver_elm327_response_complete()
{
	// This frees us up for the next request
	tx_state = TX_ST_IDLE;
}



//
// API
//
void can_driver_elm327_set_connected(bool connected)
{	
	if (connected) {
		// Trigger ELM327 controller initialization if we're idle, otherwise do nothing
		// as we're already initializing or in the connected state.
		if (op_state == OP_ST_DISCONNECTED) {
			op_state = OP_ST_INIT_ELM327;
#ifdef DEBUG_SHOW_INIT
			ESP_LOGI(TAG, "OP_ST_INIT_ELM327");
#endif
		}
	} else {
		op_state = OP_ST_DISCONNECTED;
#ifdef DEBUG_SHOW_INIT
			ESP_LOGI(TAG, "OP_ST_DISCONNECTED");
#endif
	}
}


void can_driver_elm327_tx_failed()
{
	// Only note error while executing the TX
	if ((tx_state == TX_ST_AT_CMD) || (tx_state == TX_ST_REQ_PKT)) {
		tx_state = TX_ST_ERROR;
	}
}


// Note this is called asynchronously by an interface driver
void can_driver_elm327_rx_data(char* s)
{
	char c;
	int len;
	
	// Push data into rx_buf, looking for the terminator '>'
	len = strlen(s);
	
#ifdef DEBUG_SHOW_DATA
	printf("%s RX: ", TAG);
#endif

	while (len--) {
		c = *s++;
		
#ifdef DEBUG_SHOW_DATA
	if (c == 0x0D) {
		printf("[CR]");
	} else if (c == 0x0A) {
		printf("[LF]");
	} else {
		printf("%c", c);
	}
	if (len == 0) {
		printf("\n");
	}
#endif

		rx_buf[rx_buf_push_index++] = c;
		if (rx_buf_push_index >= RX_BUFF_LEN) rx_buf_push_index = 0;
		
		if (c == '>') {
			_can_driver_elm327_process_rx_buf();
		}
	}
}



//
// Internal functions
//
static void _can_driver_elm327_task()
{
	char* s;
	int i;
	
	ESP_LOGI(TAG, "Start task");
	
	while (1) {
		while (op_state == OP_ST_INIT_ELM327) {
			// version string starts out empty
			elm327_version_string[0] = 0;
			
			// Send initialization commands
			for (i=0; i<NUM_ELM327_INIT_CMDS; i++) {
				s = elm327_init_cmd[i];
#ifdef DEBUG_SHOW_INIT
				ESP_LOGI(TAG, "Init: %s", s);
#endif
				if (!_can_driver_elm327_tx_string(TX_ST_AT_CMD, s)) {
					ESP_LOGE(TAG, "ELM327 init command failed - %s", s);
					vTaskDelay(pdMS_TO_TICKS(1000));  // Just not to overwhelm the log file
					break;  // Force a restart of initialization
				}
			}
			
			if (i == NUM_ELM327_INIT_CMDS) {
				// Successfully completed initialization
				op_state = OP_ST_CONNECTED;
#ifdef DEBUG_SHOW_INIT
				ESP_LOGI(TAG, "OP_ST_CONNECTED");
#endif

				// Version handling
				ESP_LOGI(TAG, "Found ELM327 v%s", elm327_version_string);
				elm327_is_v15 = (strcmp(elm327_version_string, "1.5") == 0);
			}
		}
		
		// Idle while not initializing
		vTaskDelay(pdMS_TO_TICKS(50));
	}
}


static void _can_driver_elm327_process_rx_buf()
{
	bool first_char = true;
	bool high_nibble = true;
	bool has_version = false;
	bool saw_data = false;
	bool success = false;
	char c;
	int n = 0;
	uint8_t data[8];
	
	// Parse all lines in this data
	while ((c = rx_buf[rx_buf_pop_index]) != '>') {
		rx_buf_pop_index += 1;
		if (rx_buf_pop_index >= RX_BUFF_LEN) rx_buf_pop_index = 0;
		
		if ((c == 0x0D) || (c == 0x0A)) {
			// CR (or NL) terminate a valid data line
			if (saw_data) {
				saw_data = false;
				can_rx_packet(prev_rsp_id, n, data);
			}
			
			// CR (or NL) always set first_char for subsequent data
			first_char = true;
			has_version = false;
			high_nibble = true;
			n = 0;
		} else {
			if (tx_state == TX_ST_AT_CMD) {
				if (first_char) {
					if ((c == 'O') || (c == 'E')) {
						// "OK" (or "ELM327" from ATZ)
						success = true;
						
						if (c == 'E') {
							// Start processing of string for version
							has_version = true;
							_can_driver_elm327_proc_version_info(c, true);
						}
					} else if (c == '?') {
						ESP_LOGE(TAG, "Unknown TX command");
						success = false;
					}
				} else if (has_version) {
					// Collect and process characters until has_version is false (next CR)
					_can_driver_elm327_proc_version_info(c, false);
				}
			} else if (tx_state == TX_ST_REQ_PKT) {
				if (_can_driver_elm327_is_hex_char(c)) {
					if (first_char) {
						// Saw data
						saw_data = true;
						success = true;
					}
					
					// Store data in our array (expect 2 hex-characters per byte)
					if (n < 8) {
						if (high_nibble) {
							data[n] = _can_driver_elm327_ascii_to_nibble(c);
							high_nibble = false;
						} else {
							data[n] = (data[n] << 4) | _can_driver_elm327_ascii_to_nibble(c);
							n += 1;
							high_nibble = true;
						}
					}
				} else if (c == ' ') {
					// Handle case where only 1 character was sent as a hex number (should not occur)
					if (!high_nibble) {
						n += 1;
						high_nibble = true;
					}
				} else if (first_char) {
					if (c == 'N') {
					// "NO DATA"
						ESP_LOGE(TAG, "No data for request");
					} else if (c == '?') {
						// Shouldn't see this but 
						ESP_LOGE(TAG, "Request received ? response");
					}
					success = false;
				}
			}
			
			// Clear flag after consuming it
			first_char = false;
		}
		
	}
	
	// Skip past the '>' character in preparation for the next response
	rx_buf_pop_index += 1;
	if (rx_buf_pop_index >= RX_BUFF_LEN) rx_buf_pop_index = 0;
	
	// Note if response was successful
	if (tx_state == TX_ST_AT_CMD) {
		tx_state = success ? TX_ST_IDLE : TX_ST_ERROR;
	} else if (tx_state == TX_ST_REQ_PKT) {
		// Success is indicated when we get all the data and _can_driver_elm327_response_complete is called.
		// That way we handle the case I saw where the ELM327 controller didn't return all the data for
		// a multi-packet response before sending '>'
		if (!success) {
			tx_state = TX_ST_ERROR;
		}
	}
}


static bool _can_driver_elm327_tx_string(int pkt_state, char* s)
{
	bool success;
	int to_count = timeout_msec;
	
	if (driverP == NULL) {
		ESP_LOGE(TAG, "Send tx string without driver");
		tx_state = TX_ST_IDLE;
		return false;
	}
	
#ifdef DEBUG_SHOW_DATA
	ESP_LOGI(TAG, "TX: %s", s);
#endif
	
	// Send the string to the interface for transmission
	if (!driverP->fcn_tx_line(s)) {
		ESP_LOGE(TAG, "Interface failed to send %s", s);
		tx_state = TX_ST_IDLE;
		return false;
	}
	
	// Set the type of command this is
	tx_state = pkt_state;
	
	// Spin waiting for the transmission to succeed or error/timeout
	while (tx_state == pkt_state) {
		vTaskDelay(pdMS_TO_TICKS(10));
		
		to_count -= 10;
		if (to_count <= 0) {
			tx_state = TX_ST_TIMEOUT;
		}
	}
	
	if (tx_state == TX_ST_TIMEOUT) {
#ifdef DEBUG_SHOW_DATA
		ESP_LOGI(TAG, "TX Timeout");
#endif
		can_if_error(CAN_ERRNO_TIMEOUT);
		success = true;
	} else if (tx_state == TX_ST_ERROR) {
#ifdef DEBUG_SHOW_DATA
		ESP_LOGE(TAG, "TX Error");
#endif
		success = false;
	} else {
#ifdef DEBUG_SHOW_DATA
		ESP_LOGI(TAG, "TX Success");
#endif
		success = true;
	}
	
	tx_state = TX_ST_IDLE;
	return success;	
}


static char _can_driver_elm327_nibble_2_ascii(uint8_t nibble)
{
	nibble = nibble & 0x0F;
	
	if (nibble <= 9) {
		return '0' + nibble;
	} else {
		return 'A' + (nibble - 10);
	}
}


static uint8_t _can_driver_elm327_ascii_to_nibble(char c)
{
	if ((c >= '0') && (c <= '9')) {
		return c - '0';
	} else if ((c >= 'a') && (c <= 'f')) {
		return 10 + (c - 'a');
	} else if ((c >= 'A') && (c <= 'F')) {
		return 10 + (c - 'A');
	} else {
		return 0;
	}
}


static bool _can_driver_elm327_is_hex_char(char c)
{
	return (((c >= '0') && (c <= '9')) || ((c >= 'a') && (c <= 'f')) || ((c >= 'A') && (c <= 'F')));
}


static void _can_driver_elm327_proc_version_info(char c, bool init)
{
	static int parse_state = 0;              // 0: looking for 'v', 1: Major number, 2: Minor number
	static int ver_index = 0;
	
	if (init) {
		parse_state = 0;
		ver_index = 0;
	} else {
		switch (parse_state) {
			case 0:
				if (c == 'v') {
					parse_state = 1;
				}
				break;
			case 1:
				if ((c >= '0') && (c <= '9')) {
					if (ver_index < MAX_ELM327_VER_LEN-1) {
						elm327_version_string[ver_index++] = c;
					}
				} else if (c == '.') {
					if (ver_index < MAX_ELM327_VER_LEN-1) {
						elm327_version_string[ver_index++] = c;
					}
					parse_state = 2;
				}
				break;
			case 2:
				if ((c >= '0') && (c <= '9')) {
					if (ver_index < MAX_ELM327_VER_LEN-1) {
						elm327_version_string[ver_index++] = c;
					}
				}
		}
		
		// Always terminate the string in case this is the last character
		elm327_version_string[ver_index] = 0;
	}
}
