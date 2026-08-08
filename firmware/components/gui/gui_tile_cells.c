/*
 * Traction battery individual cell display tile.  Display a 2D line chart with relative
 * cell voltages, minimum and maximum voltage with cell numbers and delta voltage.
 *
 * Note: We draw directly to the LVGL canvas instead of using the built-in drawing
 * functions because they are dog-slow.
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
#include "data_broker.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "gui_task.h"
#include "gui_screen_main.h"
#include "gui_tile_cells.h"
#include "gui_utilities.h"
#include "vehicle_manager.h"
#include <math.h>
#include <stdio.h>



//
// Local Constants
//
// Percentage of tile height dedicated to the graph (0-1)
#define GRAPH_H_PERCENT  0.50

// Chart colors
#define GRAPH_BG_C        lv_palette_darken(LV_PALETTE_BLUE_GREY, 3)
#define GRAPH_MID_CELL_C  lv_palette_main(LV_PALETTE_BLUE)
#define GRAPH_LOW_CELL_C  lv_palette_main(LV_PALETTE_RED)
#define GRAPH_HIGH_CELL_C lv_palette_main(LV_PALETTE_YELLOW)



//
// Local Variables
//
static const char* TAG = "gui_tile_cells";

// LVGL widgets
static lv_obj_t* tile;

static lv_obj_t* graph_img;

static lv_obj_t* min_max_lbl;
static lv_obj_t* delta_v_lbl;

// Canvas related data structures
static lv_color_t* graph_bufP;
static lv_img_dsc_t graph_img_desc;


// State
static uint16_t tile_w;
static uint16_t tile_h;
static uint16_t canvas_w;
static uint16_t canvas_h;

static bool graph_hidden;
static float graph_v_min;
static float graph_v_max;
static float graph_v_major_tick;
static lv_coord_t graph_x_offset;
static lv_coord_t graph_x_inc;
static float cell_v[DB_MAX_CELL_V_VALS];
static int num_cells;



//
// Forward declarations for internal functions
//
static void _gui_tile_cells_set_active(bool en);
static void _gui_tile_cells_setup_cell_canvas();
static void _gui_tile_cells_setup_min_max_display();
static void _gui_tile_cells_setup_delta_display();
static void _gui_tile_cells_compute_and_update_all();
static void _gui_tile_cells_update_canvas(int min_cell, int max_cell);
static void _gui_tile_cells_draw_vline(lv_coord_t x, lv_coord_t y1, lv_coord_t y2, lv_color_t c);
static void _gui_tile_cells_update_min_max_display(float min_val, int min_cell_num, float max_val, int max_cell_num);
static void _gui_tile_cells_update_delta_display(float delta_val);
static void _gui_tile_cells_set_cell_v_cb(int index, float val, bool is_final);



//
// API
//
void gui_tile_cells_init(lv_obj_t* parent_tileview, int* tile_index)
{
	uint32_t capability_mask;
	capability_mask = vm_get_supported_item_mask();
		
	gui_get_screen_size(&tile_w, &tile_h);
	canvas_h = (int) floor(GRAPH_H_PERCENT * ((float) tile_h));
	canvas_w = (int) floor(2.0 * sqrt((float)((tile_w*tile_w/4) - (canvas_h*canvas_h)/4)));
	
	// Initialize and register ourselves with our parent if we're capable of displaying something
	if ((capability_mask & DB_ITEM_HV_CELL_V) != 0) {
		// Create our object
		tile = lv_tileview_add_tile(parent_tileview, *tile_index, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
		*tile_index += 1;

		// Initialize cell voltages
		for (int i=0; i<DB_MAX_CELL_V_VALS; i++) {
			cell_v[i] = 0;
		}
		
		// Compute useful values for drawing the graph
		num_cells = vm_get_indexed_item_count(DB_ITEM_HV_CELL_V);
		graph_x_inc = (lv_coord_t) (canvas_w / num_cells);
		graph_x_offset = (lv_coord_t) (canvas_w - (graph_x_inc * num_cells)) / 2;
		vm_get_range(VM_RANGE_HV_CELLV, &graph_v_min, &graph_v_max, &graph_v_major_tick);
		
		// Setup the widgets
		_gui_tile_cells_setup_cell_canvas();
		_gui_tile_cells_setup_min_max_display();
		_gui_tile_cells_setup_delta_display();
		
		_gui_tile_cells_compute_and_update_all();
		
		gui_screen_main_register_tile(tile, _gui_tile_cells_set_active);
	}
}



//
// Internal functions
//
static void _gui_tile_cells_set_active(bool en)
{
	if (en) {
		db_register_gui_indexed_callback(DB_ITEM_HV_CELL_V, _gui_tile_cells_set_cell_v_cb);
		
		// Clear tile
		for (int i=0; i<DB_MAX_CELL_V_VALS; i++) {
			cell_v[i] = 0;
		}
		_gui_tile_cells_compute_and_update_all();
		
		// Start data flow
		vm_set_request_item_mask(DB_ITEM_HV_CELL_V);
	} else {
		// Disable the graph image object when we're being swiped away from (or unused)
		// because it renders garbage while the swipe animation is running (seems to be
		// an ESP32S3 issue between PSRAM accesses and the scrolling animation)
		lv_obj_add_flag(graph_img, LV_OBJ_FLAG_HIDDEN);
		graph_hidden = true;
	}
}


static void _gui_tile_cells_setup_cell_canvas()
{
	graph_bufP = (lv_color_t*) heap_caps_malloc(LV_CANVAS_BUF_SIZE_TRUE_COLOR(canvas_w, canvas_h), MALLOC_CAP_SPIRAM);
	if (graph_bufP != NULL) {
		// Allocate graph image rendering buffer (we draw to this)
		memset(graph_bufP, 0, (size_t) LV_CANVAS_BUF_SIZE_TRUE_COLOR(canvas_w, canvas_h));
		
		// Setup image description
		graph_img_desc.header.always_zero = 0;
		graph_img_desc.header.w = canvas_w;
		graph_img_desc.header.h = canvas_h;
		graph_img_desc.data_size = LV_CANVAS_BUF_SIZE_TRUE_COLOR(canvas_w, canvas_h);
		graph_img_desc.header.cf = LV_IMG_CF_TRUE_COLOR;
		graph_img_desc.data = (const uint8_t*) graph_bufP;
		
		// Create graph image
		graph_img = lv_img_create(tile);
		lv_obj_set_size(graph_img, canvas_w, canvas_h);
		lv_img_set_src(graph_img, &graph_img_desc);
		lv_obj_center(graph_img);
		
		// Disable it initially
		lv_obj_add_flag(graph_img, LV_OBJ_FLAG_HIDDEN);
		graph_hidden = true;
	} else {
		ESP_LOGE(TAG, "Could not allocate canvas buffer");
	}
}


static void _gui_tile_cells_setup_min_max_display()
{
	// Create the label object for the min/max value and cell number display above the canvas
	min_max_lbl = lv_label_create(tile);
	lv_label_set_long_mode(min_max_lbl, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_align(min_max_lbl, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_set_style_text_font(min_max_lbl, &lv_font_montserrat_24, LV_PART_MAIN);
	lv_obj_align(min_max_lbl, LV_ALIGN_CENTER, 0, -(canvas_h/2 + 30));
	lv_label_set_text_static(min_max_lbl, "");
}


static void _gui_tile_cells_setup_delta_display()
{
	// Create the label object for the delta value display below the canvas
	delta_v_lbl = lv_label_create(tile);
	lv_label_set_long_mode(delta_v_lbl, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_align(delta_v_lbl, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_set_style_text_font(delta_v_lbl, &lv_font_montserrat_30, LV_PART_MAIN);
	lv_obj_align(delta_v_lbl, LV_ALIGN_CENTER, 0, (canvas_h/2 + 30));
	lv_label_set_text_static(delta_v_lbl, "");
}


static void _gui_tile_cells_compute_and_update_all()
{
	float cur_min = 999;
	float cur_max = -1;
	int min_index = 0;
	int max_index = 0;
	
	for (int i=0; i<num_cells; i++) {
		if (cell_v[i] < cur_min) {
			cur_min = cell_v[i];
			min_index = i;
		} else if (cell_v[i] > cur_max) {
			cur_max = cell_v[i];
			max_index = i;
		}
	}
		
	_gui_tile_cells_update_canvas(min_index, max_index);
	_gui_tile_cells_update_min_max_display(cur_min, min_index+1, cur_max, max_index+1);
	_gui_tile_cells_update_delta_display(cur_max - cur_min);
}

	
static void _gui_tile_cells_update_canvas(int min_cell, int max_cell)
{
	lv_color_t c;
	lv_coord_t cur_x = graph_x_offset;
	lv_coord_t y_max;
	
	if (graph_bufP == NULL) return;
	
	// Erase previous graph
	memset(graph_bufP, 0, (size_t) LV_CANVAS_BUF_SIZE_TRUE_COLOR(canvas_w, canvas_h));

	// Draw new graph
	for (int i=0; i<num_cells; i++) {
		// Compute the "height" of the graph line for each cell voltage for cell voltage within the graph range
		if (cell_v[i] >= graph_v_min) {
			y_max = (lv_coord_t) (canvas_h * (cell_v[i] - graph_v_min) / (graph_v_max - graph_v_min));
			if (y_max > canvas_h) y_max = canvas_h;
			
			// Setup the line color (mark the high and low cells with distinct colors)
			if (i == min_cell) {
				c = GRAPH_LOW_CELL_C;
			} else if (i == max_cell) {
				c = GRAPH_HIGH_CELL_C;
			} else {
				c = GRAPH_MID_CELL_C;
			}
			
			// Draw the graph line
			_gui_tile_cells_draw_vline(cur_x, canvas_h - y_max, canvas_h - 1, c);
		}
		
		cur_x += graph_x_inc;
	}
	
	lv_obj_invalidate(graph_img);
}


static void _gui_tile_cells_draw_vline(lv_coord_t x, lv_coord_t y1, lv_coord_t y2, lv_color_t c)
{
	lv_color_t* bufP;
	lv_color_t* endP;
	
	// Draw down (increasing memory)
	bufP = graph_bufP + x + (y1 * canvas_w);
	endP = bufP + ((y2 - y1) * canvas_w);
	while (bufP <= endP) {
		*bufP = c;
		bufP += canvas_w;
	}
}


static void _gui_tile_cells_update_min_max_display(float min_val, int min_cell_num, float max_val, int max_cell_num)
{
	static char min_max_str[48];             // "X.XXX V (NNN) / X.XXX V (NNN)"
	
	sprintf(min_max_str, "%1.3f V (%d) / %1.3f V (%d)", min_val, min_cell_num, max_val, max_cell_num);
	lv_label_set_text_static(min_max_lbl, min_max_str);
}


static void _gui_tile_cells_update_delta_display(float delta_val)
{
	static char delta_val_str[16];            // "XXXX mV"
	int mv = round(delta_val * 1000);
	
	sprintf(delta_val_str, "%d mV", mv);
	lv_label_set_text_static(delta_v_lbl, delta_val_str);
}


static void _gui_tile_cells_set_cell_v_cb(int index, float val, bool is_final)
{
	if ((index >= 0) && (index < DB_MAX_CELL_V_VALS)) {
		cell_v[index] = val;
		if (is_final) {
			// Display graph the first time we get data
			if (graph_hidden) {
				lv_obj_clear_flag(graph_img, LV_OBJ_FLAG_HIDDEN);
				graph_hidden = false;
			}
			
			_gui_tile_cells_compute_and_update_all();
		}
	}
}
