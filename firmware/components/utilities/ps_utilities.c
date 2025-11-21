/*
 * Persistent Storage Module
 *
 * Manage the persistent storage kept in the ESP32 NVS and provide access
 * routines to it.
 *
 * Copyright 2023-2025 Dan Julio
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
#include "can_manager.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "ps_utilities.h"
#include <stdlib.h>
#include <string.h>



//
// PS Utilities internal constants
//

// Uncomment the following directive to force an NVS memory erase (but only do it for one execution)
//#define PS_ERASE_NVS

// NVS namespace
#define STORAGE_NAMESPACE "EvInfoDisplay"



//
// PS Utilities Internal variables
//
static const char* TAG = "ps_utilities";

// NVS namespace handle
static nvs_handle_t ps_handle;

// NVS Keys
static const char* config_keys[PS_NUM_CONFIGS] = {"main_key", "net_key", "ble_key"};

// Local copies
static const size_t config_data_len[PS_NUM_CONFIGS] = {sizeof(main_config_t), sizeof(net_config_t), sizeof(ble_config_t)};
static uint8_t* config_data[PS_NUM_CONFIGS];



//
// PS Utilities Forward Declarations for internal functions
//
static bool _ps_malloc_local_memory();
static bool _ps_read_config_info(int index, void* cfg);
static bool _ps_write_config_info(int index, void* cfg);
static void _ps_init_config_memory(int index);



//
// PS Utilities API
//
bool ps_init()
{
	esp_err_t err;
	size_t required_size;
	
	ESP_LOGI(TAG, "Init Persistant Storage");
	
	// Allocate RW memory to hold working copies of all config items
	if (!_ps_malloc_local_memory()) {
		ESP_LOGE(TAG, "NVS working copy allocation failed");
		return false;
	}
	
	// Initialize the NVS Storage system
	err = nvs_flash_init();
#ifdef PS_ERASE_NVS
	ESP_LOGE(TAG, "NVS Erase");
	err = nvs_flash_erase();	
	err = nvs_flash_init();
#endif

	if ((err == ESP_ERR_NVS_NO_FREE_PAGES) || (err == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
		ESP_LOGI(TAG, "NVS Erase/Init because of %d", err);
		
		// NVS partition was truncated and needs to be erased
		err = nvs_flash_erase();
		if (err != ESP_OK) {
			ESP_LOGE(TAG, "NVS Erase failed with err %d", err);
			return false;
		}
		
		// Retry init
		err = nvs_flash_init();
		if (err != ESP_OK) {
			ESP_LOGE(TAG, "NVS Init failed with err %d", err);
			return false;
		}
	}
	
	// Open NVS Storage
	err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &ps_handle);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "NVS Open %s failed with err %d", STORAGE_NAMESPACE, err);
		return false;
	}
	
	//
	// Initialize our local copies
	//   - Attempt to find the entry in NVS storage and initialize our local copy from that
	//   - Initialize our local copy with default values and use those to initialize NVS
	//     storage if it does not exist or is invalid.
	//
	for (int i=0; i<PS_NUM_CONFIGS; i++) {
		err = nvs_get_blob(ps_handle, config_keys[i], NULL, &required_size);
		if ((err != ESP_OK) && (err != ESP_ERR_NVS_NOT_FOUND)) {
			ESP_LOGE(TAG, "NVS get_blob lep size failed with err %d", err);
			return false;
		}
		if ((required_size == 0) || (required_size != config_data_len[i])) {
			if (required_size == 0) {
				ESP_LOGI(TAG, "Initializing %s", config_keys[i]);
			} else {
				ESP_LOGI(TAG, "Re-initializing %s", config_keys[i]);
			}
			_ps_init_config_memory(i);
			if (!_ps_write_config_info(i, config_data[i])) {
				ESP_LOGE(TAG, "Write %s data failed", config_keys[i]);
				return false;
			}
		} else {
			if (!_ps_read_config_info(i, config_data[i])) {
				ESP_LOGE(TAG, "Read %s data failed", config_keys[i]);
				return false;
			}
		}
	}
	
	return true;
}


bool ps_get_config(int index, void** cfg)
{
	if ((index >=0) && (index < PS_NUM_CONFIGS)) {
		// Give them a pointer to our local copy
		*cfg = config_data[index];
		return true;
	} else {
		ESP_LOGE(TAG, "Requested read of illegal config index %d", index);
		return false;
	}
}


bool ps_save_config(int index)
{
	if ((index >=0) && (index < PS_NUM_CONFIGS)) {
		// Update NVS
		if (!_ps_write_config_info(index, config_data[index])) {
			ESP_LOGE(TAG, "Failed to save %s config to NVS storage", config_keys[index]);
			return false;
		}
		
		return true;
	} else {
		ESP_LOGE(TAG, "Requested write of illegal config index %d", index);
		return false;
	}
}


bool ps_reinit_all()
{
	bool ret = true;
	
	ret &= ps_reinit_config(PS_CONFIG_TYPE_MAIN);
	ret &= ps_reinit_config(PS_CONFIG_TYPE_NET);
	ret &= ps_reinit_config(PS_CONFIG_TYPE_BLE);
	
	return ret;
}


bool ps_reinit_config(int index)
{
	if ((index >=0) && (index < PS_NUM_CONFIGS)) {
		// Reset default values to our local copy
		_ps_init_config_memory(index);
		
		// Update NVS
		if (!_ps_write_config_info(index, config_data[index])) {
			ESP_LOGE(TAG, "Failed to reset %s config to default values in NVS storage", config_keys[index]);
			return false;
		}
		
		return true;
	} else {
		ESP_LOGE(TAG, "Requested reinit of illegal config index %d", index);
		return false;
	}
}


bool ps_has_new_ap_name(const char* name)
{
	net_config_t* net_configP;
	
	// Get a config-specific pointer to the data so we can parse it
	net_configP = (net_config_t*) config_data[PS_CONFIG_TYPE_NET];
	
	return(strncmp(name, net_configP->ap_ssid, PS_SSID_MAX_LEN) != 0);
}


char ps_nibble_to_ascii(uint8_t n)
{
	n = n & 0x0F;
	
	if (n < 10) {
		return '0' + n;
	} else {
		return 'A' + n - 10;
	}
}



//
// PS Utilities internal functions
//
static bool _ps_malloc_local_memory()
{
	bool success = true;
	
	// Allocate and zero memory for each config item
	for (int i=0; i<PS_NUM_CONFIGS; i++) {
		if ((config_data[i] = calloc(config_data_len[i], sizeof(uint8_t))) == NULL) {
			ESP_LOGE(TAG, "Failed to allocate %d bytes for %s config item", config_data_len[i], config_keys[i]);
			success = false;
		}
	}
	
	return success;
}


// Assumes index is valid
static bool _ps_read_config_info(int index, void* cfg)
{
	esp_err_t ret;
	size_t len;
	
	len = config_data_len[index];
	ret = nvs_get_blob(ps_handle, config_keys[index], config_data[index], &len);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Get config blob %s failed with %d", config_keys[index], ret);
		return false;
	}
	if (len != config_data_len[index]) {
		ESP_LOGE(TAG, "Get config blob %s incorrect size %d (expected %d)", config_keys[index], len, config_data_len[index]);
		return false;
	}
	
	return true;
}


// Assumes index is valid
static bool _ps_write_config_info(int index, void* cfg)
{
	esp_err_t ret;
	
	ret = nvs_set_blob(ps_handle, config_keys[index], cfg, config_data_len[index]);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Set config blob %s failed with %d", config_keys[index], ret);
		return false;
	}
	
	ret = nvs_commit(ps_handle);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Commit config blob %s failed with %d", config_keys[index], ret);
		return false;
	}
	
	return true;
}


// This routine has to be updated if any config changes.  It assumes the index is valid.
static void _ps_init_config_memory(int index)
{
	main_config_t* main_configP;
	net_config_t* net_configP;
	ble_config_t* ble_configP;
	uint8_t sys_mac_addr[6];
	
	switch (index) {
		case PS_CONFIG_TYPE_MAIN:
			main_configP = (main_config_t*) config_data[PS_CONFIG_TYPE_MAIN];
			main_configP->bl_percent = 100;
			main_configP->config_flags = 0;
			main_configP->connection_index = CAN_MANAGER_IF_TWAI;
			main_configP->start_tile_index = -1;     // We don't know what tiles the vehicle supports yet
			strcpy(main_configP->vehicle_name, "");
			break;
		
		case PS_CONFIG_TYPE_NET:
			// Get the system's default MAC address and add 1 to match the "Soft AP" mode
			esp_efuse_mac_get_default(sys_mac_addr);
			sys_mac_addr[5] = sys_mac_addr[5] + 1;
			
			// Get a struct-friendly pointer to local storage and initialize the struct
			net_configP = (net_config_t*) config_data[PS_CONFIG_TYPE_NET];
			net_configP->sta_mode = true;
			net_configP->sta_static_ip = false;
			
			// Add our default AP SSID/Device name
			sprintf(net_configP->ap_ssid, "%s%c%c%c%c", PS_DEFAULT_AP_SSID,
				ps_nibble_to_ascii(sys_mac_addr[4] >> 4),
		    	ps_nibble_to_ascii(sys_mac_addr[4]),
		    	ps_nibble_to_ascii(sys_mac_addr[5] >> 4),
	 	    	ps_nibble_to_ascii(sys_mac_addr[5]));
	 	    	
	 	    // Default values for other fields
	 	    strcpy(net_configP->ap_pw, "12345678");
	 	    strcpy(net_configP->sta_ssid, "OBDII");
	 	    strcpy(net_configP->sta_pw, "12345678");
	 	    	
	 	    net_configP->remote_port = 35000;

			// Default IP addresses (match espressif defaults)
			net_configP->ap_ip_addr[3] = 192;
			net_configP->ap_ip_addr[2] = 168;
			net_configP->ap_ip_addr[1] = 0;
			net_configP->ap_ip_addr[0] = 1;
			net_configP->sta_ip_addr[3] = 192;
			net_configP->sta_ip_addr[2] = 168;
			net_configP->sta_ip_addr[1] = 0;
			net_configP->sta_ip_addr[0] = 11;
			net_configP->sta_netmask[3] = 255;
			net_configP->sta_netmask[2] = 255;
			net_configP->sta_netmask[1] = 255;
			net_configP->sta_netmask[0] = 0;
			break;
			
		case PS_CONFIG_TYPE_BLE:
			// This should match one of the known selections in the GUI
			ble_configP = (ble_config_t*) config_data[PS_CONFIG_TYPE_BLE];
			ble_configP->use_custom_uuid = false;
			ble_configP->use_pairing_key = false;
			strcpy(ble_configP->service_uuid, "0000");
			strcpy(ble_configP->tx_char_uuid, "0000");
			strcpy(ble_configP->rx_char_uuid, "0000");
			strcpy(ble_configP->pairing_key, "1234");
			break;
	}
}
