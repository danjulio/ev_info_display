/*
 * Main screen.
 *
 * Contains a LVGL Tile View object that displays the various information screens.
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
#ifndef GUI_SCREEN_MAIN_H
#define GUI_SCREEN_MAIN_H

#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>



//
// Constants
//

// Tiles
#define GUI_SCREEN_MAIN_TILE_TORQUE     0
#define GUI_SCREEN_MAIN_TILE_POWER      1
#define GUI_SCREEN_MAIN_TILE_ELECTRICAL 2
#define GUI_SCREEN_MAIN_TILE_TIMED      3
#define GUI_SCREEN_MAIN_TILE_SETTINGS   4

#define GUI_SCREEN_MAIN_NUM_TILES       5



//
// Function definitions for tile registration
//
typedef void (*tile_activation_handler)(bool en);



//
// API
//
lv_obj_t* gui_screen_main_init();
void gui_screen_main_set_active(bool is_active);

// From tile pages
void gui_screen_main_register_tile(lv_obj_t* tile, tile_activation_handler activate_func);

#endif /* GUI_SCREEN_MAIN_H */
