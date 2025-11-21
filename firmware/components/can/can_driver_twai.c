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
#include "can_driver_twai.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_twai.h"
#include "esp_twai_onchip.h"


//
//  Forward declarations
//

// Functions for CAN manager
static bool _can_driver_twai_init(int if_type, int req_timeout, bool can_is_500k);
static bool _can_driver_twai_connected();
static bool _can_driver_twai_tx_packet(uint32_t req_id, uint32_t rsp_id, int len, uint8_t* data);
static bool _can_driver_twai_tx_fc_packet(uint32_t req_id, int len, uint8_t* data);
static void _can_driver_twai_en_rsp_filter(bool en);
static void _can_driver_twai_response_complete();

// Internal functions
static bool _can_driver_rx_callback(twai_node_handle_t handle, const twai_rx_done_event_data_t *edata, void *user_ctx);
static bool _can_driver_state_change_callback(twai_node_handle_t handle, const twai_state_change_event_data_t *edata, void *user_ctx);
static void _can_driver_to_callback(void* arg);



//
// Driver definition
//
const can_if_driver_t can_driver_twai =
{
	"CAN TWAI Driver",
	_can_driver_twai_init,
	_can_driver_twai_connected,
	_can_driver_twai_tx_packet,
	_can_driver_twai_tx_fc_packet,
	_can_driver_twai_en_rsp_filter,
	_can_driver_twai_response_complete
};



//
// Global variables
//
static const char* TAG = "can_driver_twai";

// Driver related
static twai_node_handle_t node_hdl = NULL;
static const twai_event_callbacks_t user_cbs = {
    .on_rx_done = _can_driver_rx_callback,
    .on_state_change = _can_driver_state_change_callback,
};

// State
static bool connected = false;
static bool filter_en = false;
static int timeout_msec;

// ESP Timer
static esp_timer_handle_t req_timer;
static const esp_timer_create_args_t oneshot_timer_args = {
	.callback = &_can_driver_to_callback,
	.arg = NULL,
	.name = "CAN request timer"
};



//
// CAN manager functions
//
static bool _can_driver_twai_init(int if_type, int req_timeout, bool can_is_500k)
{
	esp_err_t ret;
	
	timeout_msec = req_timeout;
	
	twai_onchip_node_config_t node_config = {
		.io_cfg.tx = TWAI_PIN_TX,
		.io_cfg.rx = TWAI_PIN_RX,
		.io_cfg.quanta_clk_out = -1,
		.io_cfg.bus_off_indicator = -1,
		.bit_timing.bitrate = 250000,
		.tx_queue_depth = 2,
	};
	
	if (can_is_500k) {
		node_config.bit_timing.bitrate = 500000;
	}
	
	// Create a new TWAI controller driver
	if ((ret = twai_new_node_onchip(&node_config, &node_hdl)) != ESP_OK) {
		ESP_LOGE(TAG, "Driver creation failed - %d", ret);
		return false;
	}
	
	// Register our receive callback
	if ((ret = twai_node_register_event_callbacks(node_hdl, &user_cbs, NULL)) != ESP_OK) {
		ESP_LOGE(TAG, "Driver failed to register rx callback - %d", ret);
		return false;
	}
	
	twai_mask_filter_config_t mfilter_cfg = {
		.id = 0,
		.mask = 0,
		.is_ext = true,
	};
	if ((ret = twai_node_config_mask_filter(node_hdl, 0, &mfilter_cfg)) != ESP_OK) {
		ESP_LOGE(TAG, "Failed to set mask - %d", ret);
		return false;
	}
	
	// Start the driver
	if ((ret = twai_node_enable(node_hdl)) != ESP_OK) {
		ESP_LOGE(TAG, "Driver start failed - %d", ret);
		return false;
	}
	
	// Create a timer to use with transmitted packets
	if ((ret = esp_timer_create(&oneshot_timer_args, &req_timer)) != ESP_OK) {
		ESP_LOGE(TAG, "Could not create timeout timer - %d", ret);
		return false;
	}
	
	// Connected if we've made it to here
	connected = true;
	
	return true;
}


static bool _can_driver_twai_connected()
{
	return connected;
}


static bool _can_driver_twai_tx_packet(uint32_t req_id, uint32_t rsp_id, int len, uint8_t* data)
{
	bool rsp_extended_id;
	esp_err_t ret;
	
	rsp_extended_id = (rsp_id > 0x7FF);
	
	// Enable a filter for only the response packet if requested
	if (filter_en) {
		twai_mask_filter_config_t mfilter_cfg = {
			.id = rsp_id,
			.mask = (rsp_extended_id) ? 0x1FFFFFFF : 0x7FF,
			.is_ext = rsp_extended_id,
		};
		
		twai_node_disable(node_hdl);
		
		if ((ret = twai_node_config_mask_filter(node_hdl, 0, &mfilter_cfg)) != ESP_OK) {
			ESP_LOGE(TAG, "Failed to set mask 0x%x - %d", rsp_id, ret);
			return false;
		}
		
		twai_node_enable(node_hdl);
	}

	// Send the packet
	twai_frame_t tx_msg = {
		.header.id = req_id,
		.header.dlc = len,
		.header.ide = (req_id > 0x7FF),
		.buffer = data,
		.buffer_len = len
	};
	if ((ret = twai_node_transmit(node_hdl, &tx_msg, 0)) != ESP_OK) {
		ESP_LOGE(TAG, "Failed to send packet 0x%x - %d", req_id, ret);
		return false;
	}
	
	// Start timeout timer
	if (esp_timer_is_active(req_timer)) {
		(void) esp_timer_stop(req_timer);
	}
	if ((ret = esp_timer_start_once(req_timer, timeout_msec * 1000)) != ESP_OK) {
		ESP_LOGE(TAG, "Failed to start request timer - %d", ret);
	}
	
	return true;
}


// This may be called from within an ISR context
static bool _can_driver_twai_tx_fc_packet(uint32_t req_id, int len, uint8_t* data)
{
	// Send the packet
	twai_frame_t tx_msg = {
		.header.id = req_id,
		.header.dlc = len,
		.header.ide = (req_id > 0x7FF),
		.buffer = data,
		.buffer_len = len
	};
	if (twai_node_transmit(node_hdl, &tx_msg, 0) != ESP_OK) {
		return false;
	}
	
	return true;
}


static void _can_driver_twai_en_rsp_filter(bool en)
{
	filter_en = en;
	
	if (!en) {
		// Disable filter
		twai_mask_filter_config_t mfilter_cfg = {
			.id = 0,
			.mask = 0,
			.is_ext = true
		};
		twai_node_disable(node_hdl);
		if (twai_node_config_mask_filter(node_hdl, 0, &mfilter_cfg) != ESP_OK) {
			ESP_LOGE(TAG, "Disable filter failed");
		}
		twai_node_enable(node_hdl);
	}
	// No else necessary as filter is configured in tx_packet
}


static void _can_driver_twai_response_complete()
{
	// Stop the timer
    if (esp_timer_is_active(req_timer)) {
		(void) esp_timer_stop(req_timer);
	}
}



//
// Internal functions
//
static bool _can_driver_rx_callback(twai_node_handle_t handle, const twai_rx_done_event_data_t *edata, void *user_ctx)
{
    uint8_t recv_buff[8];
    twai_frame_t rx_frame = {
        .buffer = recv_buff,
        .buffer_len = sizeof(recv_buff),
    };
    
    // Push the response to the CAN manager (this is within an ISR context)
    if (twai_node_receive_from_isr(handle, &rx_frame) == ESP_OK) {
    	can_rx_packet(rx_frame.header.id, (int) twaifd_dlc2len(rx_frame.header.dlc), rx_frame.buffer);
    }
    
    return false;
}


static bool _can_driver_state_change_callback(twai_node_handle_t handle, const twai_state_change_event_data_t *edata, void *user_ctx)
{
	if (edata->new_sta == TWAI_ERROR_BUS_OFF) {
		// Initiate recovery
		(void) twai_node_recover(handle);
	}
	
	return false;
}


static void _can_driver_to_callback(void* arg)
{
	can_if_error(CAN_ERRNO_TIMEOUT);
}
