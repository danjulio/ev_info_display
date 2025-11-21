/*
 * Main system settings tile.  Display controls and update persistent storage
 * (perform system reboot after most settings are saved).
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
#ifndef GUI_TILE_SETTINGS_H
#define GUI_TILE_SETTINGS_H

#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>



//
// API
//
void gui_tile_settings_init(lv_obj_t* parent_tileview, int* tile_index);

#endif /* GUI_TILE_SETTINGS_H */