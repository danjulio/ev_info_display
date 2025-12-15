/*
 * GUI Task
 *
 * Contains functions to initialize the LVGL GUI system and a task
 * to evaluate its display related sub-tasks.  The GUI Task is responsible
 * for all access (updating) of the GUI managed by LVGL.
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
#ifndef GUI_TASK_H
#define GUI_TASK_H

#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


//
// GUI Task Constants
//

// LVGL evaluation rate (mSec)
#define GUI_LVGL_TICK_MSEC         1
#define GUI_TASK_EVAL_MSEC         10

// Screen page indicies
#define GUI_SCREEN_INTRO           0
#define GUI_SCREEN_MAIN            1
#define GUI_SCREEN_WIFI            2
#define GUI_SCREEN_BLE             3

#define GUI_NUM_MAIN_SCREEN_PAGES  4


// Notifications
//
#define GUI_NOTIFY_VEHICLE_INIT    0x00000001
#define GUI_NOTIFY_INTRO_DONE      0x00000010


//
// GUI Task externally accessible variables
//
extern TaskHandle_t task_handle_gui;



//
// GUI Task API
//
void gui_task();
void gui_set_screen_page(uint32_t page);
void gui_get_screen_size(uint16_t* w, uint16_t* h);
int32_t gui_get_init_tile_index();
void gui_set_init_tile_index(int32_t n);
bool gui_is_metric();
bool gui_has_fast_interface();


#endif /* GUI_TASK_H */