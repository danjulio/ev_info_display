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
#include "ble_utilities.h"
#include "esp_system.h"
//#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "nvs_flash.h"
#include "ps_utilities.h"
#include <string.h>



//
// BLE device description structures
//
typedef struct {
	char* device_friendly_name;
	char* device_ble_name;
	char* service_uuid;
	char* tx_char_uuid;
	char* rx_char_uuid;
} remote_ble_device_desc_t;



//
// Private constants
//

// Scan state
#define BLE_SCAN_
#define BLE_ADDR_STR_LEN 18
#define BLE_NAME_STR_LEN 80

#define BLE_DISCOVERY_TIMEOUT_MS (5000U)
#define BLE_CONNECT_TIMEOUT_MS   (10000U)

static const struct ble_gap_disc_params disc_params = {
    .passive           = 1,
    .itvl              = 0x0010,
    .window            = 0x0010,
    .filter_duplicates = 1,
};

static const struct ble_gap_conn_params conn_params = {
    .scan_itvl           = 0x0010,
    .scan_window         = 0x0010,
    .itvl_min            = 0x0010,
    .itvl_max            = 0x0020,
    .latency             = 0,
    .supervision_timeout = 0x0100,
    .min_ce_len          = 0x0010,
    .max_ce_len          = 0x0300,
};

static const uint8_t cccd_notify_enable_cfg[] = {0x01, 0x00};

// Note: Device UUIDs must be lower-case
#define NUM_KNOWN_BLE_DEVICES  1
static const remote_ble_device_desc_t known_ble_devices[NUM_KNOWN_BLE_DEVICES] = {
	{"LELink OBD-II", "OBDBLE", "ffe0", "ffe1", "ffe1"}
};



//
// Global variables
//
static const char* TAG = "ble_utilities";

// API state
static bool is_enabled = false;
static bool is_connected = false;

// Internal state
static bool svc_disc_completed = false;
static bool chr_disc_completed = false;
static bool chr_disc_started = false;
static int num_searchable_ble_devices;
static int cur_searchable_ble_device_index;
static uint16_t conn_handle;
static uint16_t tx_handle;
static uint16_t rx_handle;
static char temp_name_str[BLE_NAME_STR_LEN+1];

// System configuration
static ble_config_t* configP;



//
// Forward declarations for internal functions
//
static void _ble_client_on_reset(int reason);
static void _ble_client_on_sync(void);
static void _ble_client_host_task(void *param);
static int _ble_gap_event_cb(struct ble_gap_event *event, void *arg);
static const char* _ble_addr_to_str(const ble_addr_t *addr, char dst[BLE_ADDR_STR_LEN]);
static const char* _ble_get_ble_device_name(int index);
static const char* _ble_get_ble_device_ble_name(int index);
static const char* _ble_get_service_uuid(int index);
static const char* _ble_get_tx_char_uuid(int index);
static const char* _ble_get_rx_char_uuid(int index);
static int _ble_adv_contains_service(const struct ble_hs_adv_fields *adv_fields);
static void _ble_gap_connected_cb(uint16_t handle);
static int _ble_gatt_svc_discovered_cb(uint16_t handle, const struct ble_gatt_error *error, const struct ble_gatt_svc *service, void *arg);
static int _ble_gatt_chr_discovered_cb(uint16_t handle, const struct ble_gatt_error *error, const struct ble_gatt_chr *chr,void *arg);
static void ble_gatt_svc_chr_disc_completed_check();
static char* _ble_get_device_name(int len, const char* s);

// Necessary template for compilation
void ble_store_config_init(void);

// API callbacks
static ble_scan_complete_fcn scan_complete_cb_fcn;
static ble_rx_data_fcn rx_data_cb_fcn;


//
// API
//

/**
 * Power-on initialization of the WiFi system.  It is enabled based on start-up
 * information from persistent storage.  Returns false if any part of the initialization
 * fails.  Assumes the NVS flash subsystem has already been initialized (by PS).
 */
bool ble_init(ble_scan_complete_fcn scan_fcn, ble_rx_data_fcn rx_fcn)
{
	esp_err_t ret;
	
//	esp_log_level_set(TAG, ESP_LOG_DEBUG);
	
	// Save the calling code's callbacks
	scan_complete_cb_fcn = scan_fcn;
	rx_data_cb_fcn = rx_fcn;
	
	// Get a pointer to the persistent storage BLE configuration
	(void) ps_get_config(PS_CONFIG_TYPE_BLE, (void**) &configP);
	num_searchable_ble_devices = configP->use_custom_uuid ? NUM_KNOWN_BLE_DEVICES+1 : NUM_KNOWN_BLE_DEVICES;
	
	ret = nimble_port_init();
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to init nimble %d", ret);
		return false;
	}
	
	// Configure the host
	ble_hs_cfg.reset_cb = _ble_client_on_reset;
    ble_hs_cfg.sync_cb = _ble_client_on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
    
    // Need to have template for store (see above)
    ble_store_config_init();
    
    // Start
    nimble_port_freertos_init(_ble_client_host_task);
    
    return true;
}


bool ble_start_scan()
{
	int rc;
	
	if (!is_enabled) {
		ESP_LOGE(TAG, "Cannot start scan before enable");
		return false;
	}
	
	// Initialize discovery process tracking state
	is_connected = false;
	cur_searchable_ble_device_index = -1;
	conn_handle = 0;
	tx_handle = 0;
	rx_handle = 0;
	svc_disc_completed = false;
	chr_disc_completed = false;
	chr_disc_started   = false;
	
	// Start scanning
	rc = ble_gap_disc(0, BLE_DISCOVERY_TIMEOUT_MS, &disc_params, _ble_gap_event_cb, NULL);
	if (rc == 0) {
		ESP_LOGD(TAG, "Initiate GAP discovery");
	} else {
		ESP_LOGE(TAG, "Failed to initiate GAP discovery = %d", rc);
		return false;
	}
	
	return true;
}


bool ble_is_enabled()
{
	return is_enabled;
}


bool ble_is_connected()
{
	return is_connected;
}


bool ble_tx_data(int len, char* data)
{
	bool success = false;
	int rc;
	
	if (is_connected) {
 		rc = ble_gattc_write_flat(conn_handle, tx_handle, data, len, NULL, NULL);
 		if (rc == 0) {
 			success = true;
 		} else {
 			ESP_LOGE(TAG, "TX failed - %d", rc);
 		}
	} else {
		// Silently fail
		success = true;
	}
	
	return success;
}



//
// Internal functions
//
static void _ble_client_on_reset(int reason)
{
    ESP_LOGE(TAG, "NimBLE stack reset - reason = %d", reason);
    is_connected = false;
}


static void _ble_client_on_sync(void)
{
    is_enabled = true;
}


static void _ble_client_host_task(void *param)
{
    ESP_LOGI(TAG, "BLE Host Task Started");
    /* This function will return only when nimble_port_stop() is executed */
    nimble_port_run();

    nimble_port_freertos_deinit();
    
    vTaskDelete(NULL);
}


static int _ble_gap_event_cb(struct ble_gap_event *event, void *arg)
{
    char                     addr_str[BLE_ADDR_STR_LEN];
    int                      rc;
    struct ble_hs_adv_fields adv_fields;
    struct ble_gap_conn_desc desc;
    
    switch (event->type) {
    	case BLE_GAP_EVENT_DISC:
    		_ble_addr_to_str(&event->disc.addr, addr_str);
    		ESP_LOGD(TAG, "Discovered device: addr=%s", addr_str);
        	ESP_LOGD(TAG, "  RSSI: %d", event->disc.rssi);
        	
        	rc = ble_hs_adv_parse_fields(&adv_fields, event->disc.data, event->disc.length_data);
        	if (rc != 0) {
        		ESP_LOGE(TAG, "Failed to parse advertisement data - %d", rc);
        		break;
        	} else {
        		if (adv_fields.name_len > 0) {
        			ESP_LOGD(TAG, "Device name: %s",  _ble_get_device_name(adv_fields.name_len, (const char*) adv_fields.name));
        		}
				
				cur_searchable_ble_device_index = _ble_adv_contains_service(&adv_fields);
        		
        		if (cur_searchable_ble_device_index >= 0) {
        			// Try to connect to a matching device
        			 ESP_LOGI(TAG, "Connecting to %s (%s)...", addr_str, _ble_get_device_name(adv_fields.name_len, (const char*) adv_fields.name));
        			 ble_gap_disc_cancel();
        			 rc = ble_gap_connect(BLE_OWN_ADDR_PUBLIC, &event->disc.addr, BLE_CONNECT_TIMEOUT_MS, &conn_params, _ble_gap_event_cb, NULL);
        			 if (rc != 0) {
        			 	switch (rc) {
        			 		case BLE_HS_EALREADY:
        			 			ESP_LOGW(TAG, "Connection already in progress");
        			 			break;
        			 		case BLE_HS_EBUSY:
        			 			ESP_LOGE(TAG, "Connection not possible as scanning is still in progress");
        			 			break;
        			 		case BLE_HS_EDONE:
        			 			ESP_LOGW(TAG, "Peer already connected");
        			 			break;
        			 		default:
        			 			ESP_LOGE(TAG, "Connect error %d", rc);
        			 			
        			 			// We're done and failed
        			 			scan_complete_cb_fcn(1);
        			 	}
        			 }
        		}
        	}
    		break;
    		
    	case BLE_GAP_EVENT_DISC_COMPLETE:
    		ESP_LOGI(TAG, "Device discovery complete.");
    		scan_complete_cb_fcn(2);
    		break;
    	
    	case BLE_GAP_EVENT_CONNECT:
    		if (event->connect.status == 0) {
    			ESP_LOGI(TAG, "Connected to device. Handle: 0x%04x", event->connect.conn_handle);
    			_ble_gap_connected_cb(event->connect.conn_handle);
    		} else {
    			ESP_LOGE(TAG, "Connection to %s failed: %d", addr_str, event->connect.status);
    			// We're done and failed
				scan_complete_cb_fcn(3);
    		}
    		break;
    	
    	case BLE_GAP_EVENT_DISCONNECT:
    		is_connected = false;
    		ESP_LOGI(TAG, "Device disconnected - reason %d", event->disconnect.reason);
    		break;
    	
    	case BLE_GAP_EVENT_NOTIFY_RX:
    		// Process RX data
    		ESP_LOGD(TAG, "RX Data");
			if ((event->notify_rx.om != NULL) && (rx_data_cb_fcn != NULL) && (event->notify_rx.attr_handle == rx_handle)) {
				rx_data_cb_fcn(event->notify_rx.om->om_len, event->notify_rx.om->om_data);
			}
    		break;
    	
    	case BLE_GAP_EVENT_MTU:
    		ESP_LOGD(TAG, "MTU exchange complete. MTU size: %d", event->mtu.value);
    		break;
    	
    	case BLE_GAP_EVENT_LINK_ESTAB:
    		ESP_LOGD(TAG, "Link Established.");
    		break;
    		
    	case BLE_GAP_EVENT_CONN_UPDATE:
    		ESP_LOGD(TAG, "Connection Update - handle %u, status %d", event->conn_update.conn_handle, event->conn_update.status);
    		break;
    		
    	case BLE_GAP_EVENT_L2CAP_UPDATE_REQ:
    		ESP_LOGD(TAG, "L2CAP Update - handle %u", event->conn_update_req.conn_handle);
    		ESP_LOGD(TAG, "  peer: %u %u %u %u %u %u", 
    		         event->conn_update_req.peer_params->itvl_min,
    		         event->conn_update_req.peer_params->itvl_max,
    		         event->conn_update_req.peer_params->latency,
    		         event->conn_update_req.peer_params->supervision_timeout,
    		         event->conn_update_req.peer_params->min_ce_len,
    		         event->conn_update_req.peer_params->max_ce_len);
    		if (event->conn_update_req.self_params != NULL) {
	    		ESP_LOGD(TAG, "  self: %u %u %u %u %u %u", 
	    		         event->conn_update_req.self_params->itvl_min,
	    		         event->conn_update_req.self_params->itvl_max,
	    		         event->conn_update_req.self_params->latency,
	    		         event->conn_update_req.self_params->supervision_timeout,
	    		         event->conn_update_req.self_params->min_ce_len,
	    		         event->conn_update_req.self_params->max_ce_len);
    		}
    		break;
    	
    	case BLE_GAP_EVENT_ENC_CHANGE:
    		ESP_LOGD(TAG, "Encryption change event; status:%d", event->enc_change.status);
    		rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
    		if (rc == 0) {
				ESP_LOGD(TAG, " conn_itvl=%d conn_latency=%d supervision_timeout=%d "
	                "encrypted=%d authenticated=%d bonded=%d",
	                desc.conn_itvl, desc.conn_latency,
	                desc.supervision_timeout,
	                desc.sec_state.encrypted,
	                desc.sec_state.authenticated,
	                desc.sec_state.bonded);
    		}
    		break;
    		
    	case BLE_GAP_EVENT_DATA_LEN_CHG:
    		ESP_LOGD(TAG, "Data Length changed - TX: %u, RX: %u.", event->data_len_chg.max_tx_octets, event->data_len_chg.max_rx_octets);
    		break;
    		
    	default:
    		ESP_LOGW(TAG, "Unhandled event type: %d", event->type);
    }
    
    return 0;
}


static const char* _ble_addr_to_str(const ble_addr_t *addr, char dst[BLE_ADDR_STR_LEN])
{
    if (addr == NULL || dst == NULL)
    {
        return NULL;
    }
    snprintf(dst, BLE_ADDR_STR_LEN, "%02X:%02X:%02X:%02X:%02X:%02X", addr->val[5], addr->val[4], addr->val[3],
             addr->val[2], addr->val[1], addr->val[0]);
    return dst;
}


static const char* _ble_get_ble_device_name(int index)
{
	if (index > (NUM_KNOWN_BLE_DEVICES-1)) {
		return "Custom Device";
	} else if (index >= 0) {
		return known_ble_devices[index].device_friendly_name;
	}
	
	return NULL;
}


static const char* _ble_get_ble_device_ble_name(int index)
{
	if ((index >= 0) && (index < NUM_KNOWN_BLE_DEVICES)) {
		return known_ble_devices[index].device_ble_name;
	}
	
	return NULL;
}


static const char* _ble_get_service_uuid(int index)
{
	static char uuid_str[BLE_UUID_STR_LEN] = "0x";
	
	if (index > (NUM_KNOWN_BLE_DEVICES-1)) {
		strncpy(&uuid_str[2], configP->service_uuid, BLE_UUID_STR_LEN-2);
		return (const char*) uuid_str;
	} else if (index >= 0) {
		strncpy(&uuid_str[2], known_ble_devices[index].service_uuid, BLE_UUID_STR_LEN-2);
		return (const char*) uuid_str;
	}
	
	return NULL;
}


static const char* _ble_get_tx_char_uuid(int index)
{
	static char uuid_str[BLE_UUID_STR_LEN] = "0x";
	
	if (index > (NUM_KNOWN_BLE_DEVICES-1)) {
		strncpy(&uuid_str[2], configP->tx_char_uuid, BLE_UUID_STR_LEN-2);
		return (const char*) uuid_str;
	} else if (index >= 0) {
		strncpy(&uuid_str[2], known_ble_devices[index].tx_char_uuid, BLE_UUID_STR_LEN-2);
		return (const char*) uuid_str;
	}
	
	return NULL;
}


static const char* _ble_get_rx_char_uuid(int index)
{
	static char uuid_str[BLE_UUID_STR_LEN] = "0x";
	
	if (index > (NUM_KNOWN_BLE_DEVICES-1)) {
		strncpy(&uuid_str[2], configP->rx_char_uuid, BLE_UUID_STR_LEN-2);
		return (const char*) uuid_str;
	} else if (index >= 0) {
		strncpy(&uuid_str[2], known_ble_devices[index].rx_char_uuid, BLE_UUID_STR_LEN-2);
		return (const char*) uuid_str;
	}
	
	return NULL;
}


// Returns -1 if no match is found
static int _ble_adv_contains_service(const struct ble_hs_adv_fields *adv_fields)
{
	const char* target_uuid;
	char uuid_str[BLE_UUID_STR_LEN];
	int i;
	int j;
	
	// Look for the selected service_uuid among all the device's UUIDS
	for (i=0; i<num_searchable_ble_devices; i++) {
		target_uuid = _ble_get_service_uuid(i);
		
		// 16-bit UUIDs
		for (j = 0; j < adv_fields->num_uuids16; j++) {
			ble_uuid_to_str(&adv_fields->uuids16[j].u, uuid_str);
			ESP_LOGI(TAG, "Checking %s %s", uuid_str, target_uuid);
			if (strncmp(uuid_str, target_uuid, BLE_UUID_STR_LEN) == 0) {
				return i;
			}
		}
		
		// 32-bit UUIDs
		for (j = 0; j < adv_fields->num_uuids32; j++) {
			ble_uuid_to_str(&adv_fields->uuids32[j].u, uuid_str);
			if (strncmp(uuid_str, target_uuid, BLE_UUID_STR_LEN) == 0) {
				return i;
			}
		}
		
		// 128-bit UUIDs
		for (j = 0; j < adv_fields->num_uuids128; j++) {
			ble_uuid_to_str(&adv_fields->uuids128[j].u, uuid_str);
			if (strncmp(uuid_str, target_uuid, BLE_UUID_STR_LEN) == 0) {
				return i;
			}
		}
	}
	
	// Didn't find anything
	return -1;
}


static void _ble_gap_connected_cb(uint16_t handle)
{
	int rc;
	
	conn_handle = handle;
	
	// Start service discovery
	rc = ble_gattc_disc_all_svcs(handle, _ble_gatt_svc_discovered_cb, NULL);
	if (rc == 0) {
		ESP_LOGD(TAG, "Service discovery started");
	} else {
		ESP_LOGE(TAG, "Failed to start service discovery - %d", rc);
		scan_complete_cb_fcn(4);
	}
}


static int _ble_gatt_svc_discovered_cb(uint16_t handle, const struct ble_gatt_error *error, const struct ble_gatt_svc *service, void *arg)
{
	int rc;
	char uuid_str[BLE_UUID_STR_LEN];
    const char* service_uuid;

    if (error->status == 0) {
    	if (service != NULL) {
	        ble_uuid_to_str(&service->uuid.u, uuid_str);
	        ESP_LOGD(TAG, "Service found: UUID = %s, handle = 0x%04x", uuid_str, handle);
	        
	        service_uuid = _ble_get_service_uuid(cur_searchable_ble_device_index);
	
	        if (strncmp(uuid_str, service_uuid, BLE_UUID_STR_LEN) == 0)
	        {
	            ESP_LOGD(TAG, "Target service found: %s", uuid_str);
	            ESP_LOGD(TAG, "Starting characteristic discovery...");
	            chr_disc_started = true;
	
	            rc = ble_gattc_disc_all_chrs(handle, service->start_handle, service->end_handle,
	                                             _ble_gatt_chr_discovered_cb, NULL);
	            if (rc != 0) {
	                ESP_LOGE(TAG, "Failed to start characteristic discovery: %d", rc);
	                scan_complete_cb_fcn(5);
	            }
	        }
        }
    } else {
    	svc_disc_completed = true;
        if (error->status != BLE_HS_EDONE) {
        	ESP_LOGE(TAG, "Service discovery failed - %d", error->status);
        } else {
        	ESP_LOGD(TAG, "Service discovery complete");
        }
        ble_gatt_svc_chr_disc_completed_check();
    }

    return 0;
}


static int _ble_gatt_chr_discovered_cb(uint16_t handle, const struct ble_gatt_error *error, const struct ble_gatt_chr *chr,void *arg)
{
	int rc;
	char uuid_str[BLE_UUID_STR_LEN];
	const char* char_uuid;
	
    if (error->status == 0) {
    	if (chr != NULL) {
    		ble_uuid_to_str(&chr->uuid.u, uuid_str);
    		ESP_LOGD(TAG, "Characteristic found: UUID = %s, handle = 0x%04x", uuid_str, chr->val_handle);
    		
    		// Look for TX characteristic
    		char_uuid = _ble_get_tx_char_uuid(cur_searchable_ble_device_index);
    		if (strcmp(uuid_str, char_uuid) == 0) {
    			ESP_LOGD(TAG, "  TX characteristic found");
    			tx_handle = chr->val_handle;
    		}
    		
    		// Look for RX characteristic
    		char_uuid = _ble_get_rx_char_uuid(cur_searchable_ble_device_index);
    		if (strcmp(uuid_str, char_uuid) == 0) {
    			// Found our RX characteristic, see if we can subscribe for notifications
    			ESP_LOGD(TAG, "  Setting up notification callback for RX characteristic");
            	rc = ble_gattc_write_flat(handle,
                                         chr->val_handle + 1,  // CCCD handle is usually handle+1
                                         cccd_notify_enable_cfg, sizeof(cccd_notify_enable_cfg), NULL, NULL);
            	if (rc == 0) {
            		rx_handle = chr->val_handle;
            	} else {
            		ESP_LOGE(TAG, "Failed to subscribe to RX notifications - %d", rc);
            	}
    		}
    	}
    } else {
        ESP_LOGD(TAG, "Characteristic discovery complete");
        chr_disc_started = false;
        chr_disc_completed = true;
        ble_gatt_svc_chr_disc_completed_check();
    }

    return 0;
}


static void ble_gatt_svc_chr_disc_completed_check()
{
	bool disconnect = false;
	int rc;
	
	ESP_LOGD(TAG, "Disconnect completed check - %d, %d, %d", svc_disc_completed, chr_disc_started, chr_disc_completed);
	
	// Called with either svc_disc_completed or chr_disc_completed set
	if (chr_disc_completed) {
		// Done with characteristic discovery, check to see if we found matches
		if ((tx_handle != 0) && (rx_handle != 0)) {
			is_connected = true;
		} else {
			// Disconnect from device
			disconnect = true;
		}
	} else if (svc_disc_completed && !chr_disc_started) {
		// Done with service discovery and didn't find any matching characteristics so disconnect from device
		disconnect = true;
	}
	
	if (disconnect) {
		ESP_LOGI(TAG, "Disconnected from device");
		rc = ble_gap_terminate(conn_handle, 0x16);
		if (rc != 0) {
			if (rc == BLE_HS_ENOTCONN) {
				ESP_LOGD(TAG, "Disconnect but no connection found");
			} else {
				ESP_LOGD(TAG, "Disconnect error - %d", rc);
			}
		}
	}
	
	// Let the user know when we're done with scanning
	if (!chr_disc_started) {
		scan_complete_cb_fcn(0);
	}
}


static char* _ble_get_device_name(int len, const char* s)
{
	// Must null terminate the string
	len = (len > BLE_NAME_STR_LEN) ? BLE_NAME_STR_LEN : len;
	strcpy(temp_name_str, s);
	temp_name_str[len] = 0;
	
	return temp_name_str;
}
