/*
 * Start-up screen displayed when the system first boots.
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
#ifndef GUI_SCREEN_INTRO_H
#define GUI_SCREEN_INTRO_H

#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>



//
// Constants
//

// Display screen period
#define GUI_SCREEN_INTRO_TO_MSEC 3000



//
// API
//
lv_obj_t* gui_screen_intro_init();
void gui_screen_intro_set_active(bool is_active);

#endif /* GUI_SCREEN_INTRO_H */