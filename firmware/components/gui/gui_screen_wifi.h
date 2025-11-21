/*
 * Wifi settings screen.
 *
 * Displays Wifi OBD Adapter settings: SSID, PW, Port.  Opens keyboard popup when
 * various settings are touched to allow changes.  Provides a Save or Cancel button
 * to return to Settings tile.
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
#ifndef GUI_SCREEN_WIFI_H
#define GUI_SCREEN_WIFI_H

#include "gui_utilities.h"
#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>


//
// API
//
lv_obj_t* gui_screen_wifi_init();
void gui_screen_wifi_set_active(bool is_active);

#endif /* GUI_SCREEN_WIFI_H */
