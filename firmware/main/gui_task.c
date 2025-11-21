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
#include "data_broker.h"
#include "disp_driver.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_freertos_hooks.h"
#include "gt911.h"
#include "gui_task.h"
#include "gui_screen_ble.h"
#include "gui_screen_intro.h"
#include "gui_screen_main.h"
#include "gui_screen_wifi.h"
#include "lvgl.h"
#include "mem_fb.h"
#include "ST7701S.h"
#include "touch_driver.h"
#include "I2C_Driver.h"
#include "TCA9554PWR.h"
#include "ps_utilities.h"



//
// Private constants
//
#define Notification(var, mask) ((var & mask) == mask)

// Period between new tile display and persistent memory update
#define TILE_PS_UPDATE_MSEC (15 * 1000)

// Uncomment to enable screen dumps
//   Note: this dumps the screen raw hex data to the USB debug log output when
//   the button attached to IO0 is pressed.  The GUI is frozen while the dump is
//   occurring.  The intent is to capture the log file with a special program
//   that will display the screen image for capture the attached computer.
//#define ENABLE_SCREENDUMP



//
// Global variables
//

static const char* TAG = "gui_task";

// Dual display update buffers to allow DMA/SPI transfer of one while the other is updated
static void* lvgl_disp_buf1 = NULL;
static void* lvgl_disp_buf2 = NULL;
static lv_disp_draw_buf_t lvgl_draw_buf;

// Display driver
static lv_disp_drv_t lvgl_disp_drv;

// Touchscreen driver
static lv_indev_drv_t lvgl_indev_drv;

// Main screen pages
static lv_obj_t* screen_pages[GUI_NUM_MAIN_SCREEN_PAGES];

// Default displayed tile state
static int32_t cur_tile_index;
static lv_timer_t* tile_ps_update_timer = NULL;

// Task handle
TaskHandle_t task_handle_gui;

// Configuration
static main_config_t* configP;

// Notifications
static bool saw_end_of_intro = false;
static bool saw_vehicle_init = false;



//
// GUI Task internal function forward declarations
//
static void _gui_notification_handler();
static void _gui_lvgl_init();
static void _gui_init_screens();
static void _lv_tick_callback();
static void _gui_ps_update_timer_cb(lv_timer_t* timer);
static bool _gui_screendump_button_eval();
static void _gui_do_screendump();


//
// GUI Task API
//

void gui_task()
{
	ESP_LOGI(TAG, "Start task");
	
	// Get a pointer to the system configuration
	if (!ps_get_config(PS_CONFIG_TYPE_MAIN, (void**) &configP)) {
		ESP_LOGE(TAG, "Get configuration failed");
		vTaskDelete(NULL);
	}
	
	// Initialize LVGL
	_gui_lvgl_init();
	
	// Set the initial screen brightness
	if (configP->bl_percent < 10) {
		disp_driver_set_bl(10);
	} else if (configP->bl_percent > 100) {
		disp_driver_set_bl(100);
	} else {
		disp_driver_set_bl((uint8_t) configP->bl_percent);
	}
	
	// Initialize the main display objects
	_gui_init_screens();
	
	// Set the initial display
	gui_set_screen_page(GUI_SCREEN_INTRO);
	
	// GUI Task
	while (1) {
		lv_task_handler();
		lv_timer_handler();
		
		// Evaluate data broker to get updated values
		db_gui_eval();
		
		// Handle incoming notifications
		_gui_notification_handler();
		
#ifdef ENABLE_SCREENDUMP
		if (_gui_screendump_button_eval()) {
			_gui_do_screendump();
		}
#endif
		
		vTaskDelay(pdMS_TO_TICKS(GUI_TASK_EVAL_MSEC));
	}
}


void gui_set_screen_page(uint32_t page)
{
	if (page >= GUI_NUM_MAIN_SCREEN_PAGES) return;
	
	// Let pages know their activation state
	gui_screen_intro_set_active(page == GUI_SCREEN_INTRO);
	gui_screen_main_set_active(page == GUI_SCREEN_MAIN);
	gui_screen_wifi_set_active(page == GUI_SCREEN_WIFI);
	gui_screen_ble_set_active(page == GUI_SCREEN_BLE);
	
	// Set the new page
	lv_scr_load(screen_pages[page]);
}


void gui_get_screen_size(uint16_t* w, uint16_t* h)
{
	*w = LCD_H_RES;
	*h = LCD_V_RES;
}


int32_t gui_get_init_tile_index()
{
	cur_tile_index = configP->start_tile_index;
	return cur_tile_index;
}


void gui_set_init_tile_index(int32_t n)
{
	ESP_LOGI(TAG, "Set tile index = %d", n);
	cur_tile_index = n;
	
	// Start the changed tile timer.  We update persistent storage after it expires.
	// This allows us to not excessively write persistent storage if the user is flipping
	// between tiles.
	if (tile_ps_update_timer == NULL) {
		// Create a new timer
		tile_ps_update_timer = lv_timer_create(_gui_ps_update_timer_cb, TILE_PS_UPDATE_MSEC, NULL);
		lv_timer_set_repeat_count(tile_ps_update_timer, 1);
	} else {
		// Restart the already running timer
		lv_timer_reset(tile_ps_update_timer);
	}
}


bool gui_is_metric()
{
	return (configP->config_flags & PS_MAIN_FLAG_METRIC) == PS_MAIN_FLAG_METRIC;
}



//
// GUI Task Internal functions
//
static void _gui_notification_handler()
{
	uint32_t notification_value;
	
	// Look for incoming notifications (clear them upon reading)
	if (xTaskNotifyWait(0x00, 0xFFFFFFFF, &notification_value, 0)) {
		if (Notification(notification_value, GUI_NOTIFY_VEHICLE_INIT)) {
			saw_vehicle_init = true;
			if (saw_end_of_intro) {
				gui_set_screen_page(GUI_SCREEN_MAIN);
			}
		}
		
		if (Notification(notification_value, GUI_NOTIFY_INTRO_DONE)) {
			saw_end_of_intro = true;
			if (saw_vehicle_init) {
				gui_set_screen_page(GUI_SCREEN_MAIN);
			}
		}
	}
}


static void _gui_lvgl_init()
{
	// Initialize lvgl
	lv_init();
	
	// Interface and driver initialization
	disp_driver_init(&lvgl_disp_drv);
	touch_driver_init();
	
	// Get the display buffers
#if CONFIG_USE_PSRAM_BUFFER
    ESP_LOGI(TAG, "Allocate full LVGL draw buffers from PSRAM");
    lvgl_disp_buf1 = heap_caps_malloc(LCD_H_RES * LCD_V_RES, MALLOC_CAP_SPIRAM);
    assert(lvgl_disp_buf1);
    lvgl_disp_buf2 = heap_caps_malloc(LCD_H_RES * LCD_V_RES, MALLOC_CAP_SPIRAM);
    assert(lvgl_disp_buf2);
    lv_disp_draw_buf_init(&lvgl_draw_buf, lvgl_disp_buf1, lvgl_disp_buf2, LCD_H_RES * LCD_V_RES);
#else
    ESP_LOGI(TAG, "Allocate partial LVGL draw buffers from DRAM");
    lvgl_disp_buf1 = heap_caps_aligned_alloc(32, LCD_H_RES * LCD_V_RES * 2 / 10, MALLOC_CAP_DMA);
    assert(lvgl_disp_buf1);
    lvgl_disp_buf2 = heap_caps_aligned_alloc(32, LCD_H_RES * LCD_V_RES * 2 / 10, MALLOC_CAP_DMA);
    assert(lvgl_disp_buf2);
    lv_disp_draw_buf_init(&lvgl_draw_buf, lvgl_disp_buf1, lvgl_disp_buf2, LCD_H_RES * LCD_V_RES / 10);
#endif
	
	// Install the display driver
	lv_disp_drv_init(&lvgl_disp_drv);
	lvgl_disp_drv.hor_res = LCD_H_RES;
    lvgl_disp_drv.ver_res = LCD_V_RES;
    lvgl_disp_drv.flush_cb = disp_driver_flush;
    lvgl_disp_drv.draw_buf = &lvgl_draw_buf;
    lvgl_disp_drv.user_data = panel_handle;
    lv_disp_t *disp = lv_disp_drv_register(&lvgl_disp_drv);
    
    // Install the touchscreen driver
    lv_indev_drv_init (&lvgl_indev_drv);
    lvgl_indev_drv.type = LV_INDEV_TYPE_POINTER;
    lvgl_indev_drv.disp = disp;
    lvgl_indev_drv.read_cb = touch_driver_read;
    lv_indev_drv_register(&lvgl_indev_drv);

    // Hook LVGL's timebase to the CPU system tick so it can keep track of time
    esp_register_freertos_tick_hook(_lv_tick_callback);
}


static void _gui_init_screens()
{
	screen_pages[GUI_SCREEN_INTRO] = gui_screen_intro_init();
	screen_pages[GUI_SCREEN_MAIN] = gui_screen_main_init();
	screen_pages[GUI_SCREEN_WIFI] = gui_screen_wifi_init();
	screen_pages[GUI_SCREEN_BLE] = gui_screen_ble_init();
}


static void IRAM_ATTR _lv_tick_callback()
{
	lv_tick_inc(portTICK_PERIOD_MS);
}


static void _gui_ps_update_timer_cb(lv_timer_t* timer)
{
	// Update persistent storage if necessary (don't save tile if we're displaying the settings page)
	if ((cur_tile_index != configP->start_tile_index) && (cur_tile_index != GUI_SCREEN_MAIN_TILE_SETTINGS)) {
		configP->start_tile_index = cur_tile_index;
		ESP_LOGI(TAG, "Save start tile");
		ps_save_config(PS_CONFIG_TYPE_MAIN);
	}
	
	// Note single-shot timer has been deleted
	tile_ps_update_timer = NULL;
}


#ifdef ENABLE_SCREENDUMP
static bool _gui_screendump_button_eval()
{
	static bool prev_btn = false;
	static bool btn_down = false;
	bool cur_btn;
	bool btn_pressed = false;
	
	// Get current button state
	cur_btn = (gpio_get_level(0) == 0);
	
	// Debounce and look for press indication
	if (!btn_down && cur_btn && prev_btn) {
		btn_pressed = true;
		btn_down = true;
	}
	if (btn_down && !cur_btn && !prev_btn) {
		btn_down = false;
	}
	prev_btn = cur_btn;
	
	return btn_pressed;
}


// This task blocks gui_task
static void _gui_do_screendump()
{
	char line_buf[161];   // Large enough for 32 16-bit hex values with a space between them
	int i, j, n;
	int len = MEM_FB_W * MEM_FB_H;
	uint16_t* fb;
	
	// Configure the display driver to render to the screendump frame buffer
	disp_driver_en_dump(true);
	
	// Force LVGL to redraw the entire screen (to the screendump frame buffer)
	lv_obj_invalidate(lv_scr_act());
	lv_refr_now(lv_disp_get_default());
	
	// Reconfigure the driver back to the LCD
	disp_driver_en_dump(false);
	
	// Dump the fb
	fb = (uint16_t*) mem_fb_get_buffer();
	i = 0;
	while (i < len) {
		n = 0;
		for (j=0; j<16; j++) {
			sprintf(line_buf + n, "%x ", *fb++);
			n = strlen(line_buf);
		}
		i += j;
		printf("%s: FB: %s\n", TAG, line_buf);
		vTaskDelay(pdMS_TO_TICKS(20));
	}
}
#endif