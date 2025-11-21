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
#ifndef PS_UTILITIES_H
#define PS_UTILITIES_H

#include <stdbool.h>
#include <stdint.h>



//
// PS Utilities Constants
//

//
// Configuration types
#define PS_NUM_CONFIGS           3

#define PS_CONFIG_TYPE_MAIN      0
#define PS_CONFIG_TYPE_NET       1
#define PS_CONFIG_TYPE_BLE       2

// Main configuration flags
#define PS_MAIN_FLAG_METRIC      0x00000001

// Field lengths
#define PS_VEHICLE_NAME_MAX_LEN  32
#define PS_SSID_MAX_LEN          32
#define PS_PW_MAX_LEN            63
#define PS_BLE_UUID_STR_LEN      37
#define PS_BLE_PAIRING_KEY_LEN   16

// Base part of the default SSID/Device name - the last 4 nibbles of the ESP32's
// mac address are appended as ASCII characters
#define PS_DEFAULT_AP_SSID      "EvInfoDisp-"



//
// PS Utilities config typedefs
//
typedef struct {
	uint32_t bl_percent;                          // 0 - 100
	uint32_t config_flags;
	uint32_t connection_index;                    // CAN Manager interface type
	int32_t start_tile_index;                     // -1 indicates GUI should select first available tile
	char vehicle_name[PS_VEHICLE_NAME_MAX_LEN+1]; // Null indicates select first available vehicle
} main_config_t;

typedef struct {
	bool sta_mode;                                // 0: AP mode, 1: STA mode
	bool sta_static_ip;                           // In station mode: 0: DHCP-served IP, 1: Static IP
	char ap_ssid[PS_SSID_MAX_LEN+1];
	char sta_ssid[PS_SSID_MAX_LEN+1];
	char ap_pw[PS_PW_MAX_LEN+1];
	char sta_pw[PS_PW_MAX_LEN+1];
	uint16_t remote_port;
	uint8_t ap_ip_addr[4];
	uint8_t sta_ip_addr[4];
	uint8_t sta_netmask[4];
} net_config_t;

typedef struct {
	bool use_custom_uuid;                        // Set to try the UUIDs here along with known devices
	bool use_pairing_key;                        // Set to enable pairing keys, clear for "just works"
	char service_uuid[PS_BLE_UUID_STR_LEN];      // Service UUID
	char tx_char_uuid[PS_BLE_UUID_STR_LEN];      // Transmit (W) characteristic UUID
	char rx_char_uuid[PS_BLE_UUID_STR_LEN];      // Receive (R) notification characteristic UUID
	char pairing_key[PS_BLE_PAIRING_KEY_LEN+1];  // Optional pairing key to send
} ble_config_t;



//
// PS Utilities API
//
bool ps_init();
bool ps_get_config(int index, void** cfg);
bool ps_save_config(int index);
bool ps_reinit_all();
bool ps_reinit_config(int index);
bool ps_has_new_ap_name(const char* name);
char ps_nibble_to_ascii(uint8_t n);

#endif /* PS_UTILITIES_H */
