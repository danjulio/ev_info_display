/*
 * Timed 0-60MPH/0-100KPH display tile.  Display speed and implement a race-timer like
 * function for timed speed runs.
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
#include "Buzzer.h"
#include "data_broker.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "gui_task.h"
#include "gui_screen_main.h"
#include "gui_tile_timed.h"
#include "gui_utilities.h"
#include "vehicle_manager.h"
#include <math.h>
#include <stdio.h>



//
// Private constants
//

// Time between "Christmas Tree" amber and green LED flashes
#define COUNTDOWN_STEP_MSEC   500

// Final "Christmas tree" status display time
#define COUNTDOWN_DONE_MSEC   2000

// Beep intervals
#define COUNTDOWN_BEEP_MSEC   150
#define TEST_GO_BEEP_MSEC     500
#define TEST_END_BEEP_MSEC    500

// Timer state evaluation interval (LVGL system must evaluate timers at this rate or faster)
#define TIMER_EVAL_MSEC       10

// Maximum timer value (speed test ends after this period without reaching 60 MPH)
#define TEST_TIMEOUT_MSEC     (15 * 1000)

// Speed goal
#define TEST_END_MPH          60
#define TEST_END_KPH          100

// Speedometer range based on units
#define METER_RANGE_MPH       100
#define METER_RANGE_KPH       160

// Timer state machine states
#define TIMER_STATE_IDLE      0
#define TIMER_STATE_STARTERR1 1
#define TIMER_STATE_STARTERR2 2
#define TIMER_STATE_STARTERR3 3
#define TIMER_STATE_TRIGGERED 4
#define TIMER_STATE_A1        5
#define TIMER_STATE_A2        6
#define TIMER_STATE_A3        7
#define TIMER_STATE_RUNNING1  8
#define TIMER_STATE_RUNNING2  9
#define TIMER_STATE_DONE      10
#define TIMER_STATE_ERROR     11

// "Christmas tree" LED brightnesses
#define XMAS_LED_BRIGHT       255

// "Christmas tree" LED display state
#define XMAS_STATE_OFF        0
#define XMAS_STATE_A1         1
#define XMAS_STATE_A2         2
#define XMAS_STATE_A3         3
#define XMAS_STATE_G          4
#define XMAS_STATE_R          5



//
// Local Variables
//
static lv_obj_t* tile;

static lv_obj_t* meter_speed;
static lv_obj_t* speed_arc;
static lv_obj_t* speed_val_lbl;

static lv_obj_t* timer_lbl;           // Format SEC.HUNDREDS - 4.2
static lv_obj_t* start_btn;
static lv_obj_t* start_btn_lbl;

static lv_obj_t* led_a1;
static lv_obj_t* led_a2;
static lv_obj_t* led_a3;
static lv_obj_t* led_g;
static lv_obj_t* led_r;

static lv_anim_t speed_animation;   // Animator for smooth meter movement between values

static lv_timer_t* beep_timer = NULL;
static lv_timer_t* run_eval_timer = NULL;

// Vehicle capability flags
static bool has_speed;

// Meter upper range
static int16_t meter_range;

// Speed test goal in selected units
static int32_t speed_goal;

// State
static bool units_metric;
static bool false_start;
static int timer_state;
static int timer_countdown;
static uint16_t tile_w;
static uint16_t tile_h;
static int32_t speed;                // KPH or MPH
static uint32_t elapsed_deciseconds;
static int64_t start_timestamp;      // ESP32 system uSec since start



//
// Forward declarations for internal functions
//
static void _gui_tile_timed_set_active(bool en);
static void _gui_tile_timed_setup_vehicle();
static void _gui_tile_timed_setup_speed_meter();
static void _gui_tile_timed_setup_timer_display();
static void _gui_tile_timed_setup_start_btn();
static void _gui_tile_timed_setup_xmas_tree();
static void _gui_tile_timed_update_speed_meter(int32_t val, bool immediate);
static void _gui_tile_timed_set_speed_meter_cb(void* indic, int32_t val);
static void _gui_tile_timed_update_timer_display(uint32_t msec);
static void _gui_tile_timed_update_xmas_tree(int state);
static void _gui_tile_timed_start_beep(uint32_t period);
static void _gui_tile_timed_btn_cb(lv_event_t* e);
static void _gui_tile_timed_beep_timer_cb(lv_timer_t* timer);
static void _gui_tile_timed_run_timer_cb(lv_timer_t* timer);
static void _gui_tile_timed_set_timer_state(int state);
static void _gui_tile_timed_speed_cb(float val);



//
// API
//
void gui_tile_timed_init(lv_obj_t* parent_tileview, int* tile_index)
{
	// Create our object
	tile = lv_tileview_add_tile(parent_tileview, *tile_index, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
	*tile_index += 1;
	
	gui_get_screen_size(&tile_w, &tile_h);
	
	// Determine our capabilities
	_gui_tile_timed_setup_vehicle();
	
	// First get our display units
	units_metric = gui_is_metric();
	
	// Then set units-specific items before we create meters
	meter_range = (units_metric) ? METER_RANGE_KPH : METER_RANGE_MPH;
	speed_goal = (units_metric) ? TEST_END_KPH : TEST_END_MPH;
	
	// Create display objects
	if (has_speed) {
		_gui_tile_timed_setup_speed_meter();
		_gui_tile_timed_setup_timer_display();
		_gui_tile_timed_setup_start_btn();
		_gui_tile_timed_setup_xmas_tree();
		
		// Create our evaluation timer
		run_eval_timer = lv_timer_create(_gui_tile_timed_run_timer_cb, TIMER_EVAL_MSEC, NULL);
		lv_timer_set_repeat_count(run_eval_timer, -1);
		lv_timer_pause(run_eval_timer);
	}
	
	// Register ourselves with our parent if we're capable of displaying something
	if (has_speed) {
		gui_screen_main_register_tile(tile, _gui_tile_timed_set_active);
	}
}



//
// Internal functions
//
static void _gui_tile_timed_set_active(bool en)
{
	uint32_t req_mask = 0;
	
	if (en) {		
		// Setup to receive data we require
		if (has_speed) {
			db_register_gui_callback(DB_ITEM_SPEED, _gui_tile_timed_speed_cb);
			req_mask |= DB_ITEM_SPEED;

			// Start data flow
			vm_set_request_item_mask(req_mask);
			
			// Start our evaluation timer
			lv_timer_resume(run_eval_timer);
			
			// Set initial state
			speed = 0;
			timer_state = TIMER_STATE_IDLE;
			_gui_tile_timed_update_speed_meter(0, true);
			_gui_tile_timed_update_timer_display(0);
			_gui_tile_timed_update_xmas_tree(XMAS_STATE_OFF);
		}		
		
		// Initialize the update interval timer for meter animations (this will change
		// to reflect real system timing)
		gui_utility_init_update_time(100);
	} else {
		// Pause our evaluation timer
		lv_timer_pause(run_eval_timer);
	}
}


static void _gui_tile_timed_setup_vehicle()
{
	uint32_t capability_mask;
	
	capability_mask = vm_get_supported_item_mask();
	
	has_speed = (capability_mask & DB_ITEM_SPEED) != 0;
}


static void _gui_tile_timed_setup_speed_meter()
{
	uint16_t meter_ticks = gui_utility_setup_large_270_meter_ticks(0, meter_range);
	
	// Meter background
    meter_speed = lv_meter_create(tile);
    lv_obj_center(meter_speed);
    lv_obj_set_size(meter_speed, tile_w, tile_h);

    // Remove the circle from the middle
    lv_obj_remove_style(meter_speed, NULL, LV_PART_INDICATOR);
    
    // Add a scale first
    lv_meter_scale_t * scale = lv_meter_add_scale(meter_speed);
    lv_meter_set_scale_ticks(meter_speed, scale, meter_ticks, 2, 20, lv_palette_main(LV_PALETTE_GREY));
    lv_meter_set_scale_major_ticks(meter_speed, scale, 2, 3, 30, lv_color_hex3(0xeee), 20);
    lv_meter_set_scale_range(meter_speed, scale, 0, meter_range, 270, 135);
    lv_obj_set_style_text_font(meter_speed, &lv_font_montserrat_18, LV_PART_MAIN);
    
    // Create the green meter arc
    speed_arc = lv_arc_create(tile);
    lv_obj_center(speed_arc);
	lv_obj_set_size(speed_arc, tile_w-10, tile_h-10);
    lv_arc_set_rotation(speed_arc, 135);
	lv_arc_set_bg_angles(speed_arc, 0, 270);
	lv_arc_set_range(speed_arc, 0, meter_range);
	lv_obj_set_style_bg_color(speed_arc, lv_palette_main(LV_PALETTE_BLUE_GREY), LV_PART_INDICATOR);
	lv_obj_remove_style(speed_arc, NULL, LV_PART_KNOB);
	lv_obj_clear_flag(speed_arc, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_set_style_arc_color(speed_arc, lv_palette_main(LV_PALETTE_GREEN), LV_PART_INDICATOR);
	
	// Label the meter with its units
	speed_val_lbl = lv_label_create(tile);
	lv_label_set_long_mode(speed_val_lbl, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_align(speed_val_lbl, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_set_style_text_font(speed_val_lbl, &lv_font_montserrat_30, LV_PART_MAIN);
//	lv_obj_align(speed_val_lbl, LV_ALIGN_BOTTOM_MID, 0, -20);
	lv_obj_align(speed_val_lbl, LV_ALIGN_CENTER, 0, -(tile_h/4) + 10);
	
	// Initialize the meter to 0
	speed = -1.0;   // Force update
	_gui_tile_timed_update_speed_meter(0, true);
}


static void _gui_tile_timed_setup_timer_display()
{
	timer_lbl = lv_label_create(tile);
	lv_label_set_long_mode(timer_lbl, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_align(timer_lbl, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_set_style_text_font(timer_lbl, &lv_font_montserrat_48, LV_PART_MAIN);
//	lv_obj_align(timer_lbl, LV_ALIGN_CENTER, 0, -(tile_h/4) + 60);
	lv_obj_align(timer_lbl, LV_ALIGN_CENTER, 0, -40);
	
	// Initialize display to 0.0
	elapsed_deciseconds = 1; // Force update
	_gui_tile_timed_update_timer_display(0);
}


static void _gui_tile_timed_setup_start_btn()
{
	uint16_t btn_w = tile_w / 4;
	uint16_t btn_h = tile_h / 10;
	
	start_btn = lv_btn_create(tile);
	lv_obj_set_size(start_btn, btn_w, btn_h);
	lv_obj_add_event_cb(start_btn, _gui_tile_timed_btn_cb, LV_EVENT_ALL, NULL);
    lv_obj_align(start_btn, LV_ALIGN_BOTTOM_MID, 0, -80);
	
	start_btn_lbl = lv_label_create(start_btn);
	lv_obj_set_style_text_font(start_btn_lbl, &lv_font_montserrat_30, LV_PART_MAIN);
	lv_label_set_text(start_btn_lbl, "Start");
	lv_obj_center(start_btn_lbl);
}


static void _gui_tile_timed_setup_xmas_tree()
{
	uint16_t led_diameter = tile_w / 32;
	uint16_t led_spacing = led_diameter * 3;
	uint16_t align_offset = 30;
	
	led_a1 = lv_led_create(tile);
	lv_obj_align(led_a1, LV_ALIGN_CENTER, -(2 * led_spacing), align_offset);
	lv_led_set_color(led_a1, lv_palette_main(LV_PALETTE_AMBER));
	lv_led_set_brightness(led_a1, XMAS_LED_BRIGHT);
	
	led_a2 = lv_led_create(tile);
	lv_obj_align(led_a2, LV_ALIGN_CENTER, -(1 * led_spacing), align_offset);
	lv_led_set_color(led_a2, lv_palette_main(LV_PALETTE_AMBER));
	lv_led_set_brightness(led_a2, XMAS_LED_BRIGHT);
	
	led_a3 = lv_led_create(tile);
	lv_obj_align(led_a3, LV_ALIGN_CENTER, 0, align_offset);
	lv_led_set_color(led_a3, lv_palette_main(LV_PALETTE_AMBER));
	lv_led_set_brightness(led_a3, XMAS_LED_BRIGHT);
	
	led_g = lv_led_create(tile);
	lv_obj_align(led_g, LV_ALIGN_CENTER, (1 * led_spacing), align_offset);
	lv_led_set_color(led_g, lv_palette_main(LV_PALETTE_GREEN));
	lv_led_set_brightness(led_g, XMAS_LED_BRIGHT);
	
	led_r = lv_led_create(tile);
	lv_obj_align(led_r, LV_ALIGN_CENTER, (2 * led_spacing), align_offset);
	lv_led_set_color(led_r, lv_palette_main(LV_PALETTE_RED));
	lv_led_set_brightness(led_r, XMAS_LED_BRIGHT);
	
	_gui_tile_timed_update_xmas_tree(XMAS_STATE_OFF);
}


static void _gui_tile_timed_update_speed_meter(int32_t val, bool immediate)
{
	static char speed_str[16];
	uint32_t anim_time;
	
	// Update the label immediately
	if (units_metric) {
		sprintf(speed_str, "%ld kph", val);
	} else {	
		sprintf(speed_str, "%ld mph", val);
	}
	lv_label_set_text_static(speed_val_lbl, speed_str);
	
	if (immediate) {
		_gui_tile_timed_set_speed_meter_cb(speed_arc, val);
	} else {
		// Start an animation to the new value for the meter indicator
		anim_time = gui_utility_get_update_period() - 20;   // Just slightly faster than the average update interval
		lv_anim_init(&speed_animation);
		lv_anim_set_exec_cb(&speed_animation, _gui_tile_timed_set_speed_meter_cb);
		lv_anim_set_var(&speed_animation, speed_arc);
		lv_anim_set_time(&speed_animation, anim_time);
		lv_anim_set_values(&speed_animation, speed, val);
		lv_anim_start(&speed_animation);
	}
}


static void _gui_tile_timed_set_speed_meter_cb(void* indic, int32_t val)
{
	lv_arc_set_value(speed_arc, (int16_t) val);
}


static void _gui_tile_timed_update_timer_display(uint32_t msec)
{
	static char buf[32];  // Room for 2 32-bit integers (keep compiler happy) but we expect "XX.X sec"
	uint32_t sec;
	uint32_t msec_100;
	uint32_t decisecond;
	
	// Displayed time operates in 100 mSec increments
	decisecond = msec / 100;
	if ((msec % 100) >= 50) {
		// Round up to next value
		decisecond += 1;
	}
	
	if (decisecond != elapsed_deciseconds) {
		// Update displayed value
		sec = decisecond / 10;
		msec_100 = decisecond % 10;
		sprintf(buf, "%lu.%lu sec", sec, msec_100);
		lv_label_set_text_static(timer_lbl, buf);
		elapsed_deciseconds = decisecond;
	}
}


static void _gui_tile_timed_update_xmas_tree(int state)
{
	switch (state) {
		case XMAS_STATE_OFF:
			lv_led_off(led_a1);
			lv_led_off(led_a2);
			lv_led_off(led_a3);
			lv_led_off(led_g);
			lv_led_off(led_r);
			break;
		case XMAS_STATE_A1:
			lv_led_on(led_a1);
			lv_led_off(led_a2);
			lv_led_off(led_a3);
			lv_led_off(led_g);
			lv_led_off(led_r);
			break;
		case XMAS_STATE_A2:
			lv_led_off(led_a1);
			lv_led_on(led_a2);
			lv_led_off(led_a3);
			lv_led_off(led_g);
			lv_led_off(led_r);
			break;
		case XMAS_STATE_A3:
			lv_led_off(led_a1);
			lv_led_off(led_a2);
			lv_led_on(led_a3);
			lv_led_off(led_g);
			lv_led_off(led_r);
			break;
		case XMAS_STATE_G:
			lv_led_off(led_a1);
			lv_led_off(led_a2);
			lv_led_off(led_a3);
			lv_led_on(led_g);
			lv_led_off(led_r);
			break;
		case XMAS_STATE_R:
			lv_led_off(led_a1);
			lv_led_off(led_a2);
			lv_led_off(led_a3);
			lv_led_off(led_g);
			lv_led_on(led_r);
			break;
	}
}


static void _gui_tile_timed_start_beep(uint32_t period)
{
	
	beep_timer = lv_timer_create(_gui_tile_timed_beep_timer_cb, period, NULL);
	lv_timer_set_repeat_count(beep_timer, 1);
	Buzzer_On();
}


static void _gui_tile_timed_btn_cb(lv_event_t* e)
{
	lv_event_code_t code = lv_event_get_code(e);
	
	if (code == LV_EVENT_CLICKED) {
		if (timer_state == TIMER_STATE_IDLE) {
			if (speed == 0) {
				// Start timed speed run
				_gui_tile_timed_set_timer_state(TIMER_STATE_TRIGGERED);
			} else {
				// Note start error
				_gui_tile_timed_set_timer_state(TIMER_STATE_STARTERR1);
			}
		}
	}
}


static void _gui_tile_timed_beep_timer_cb(lv_timer_t* timer)
{
	Buzzer_Off();
}


static void _gui_tile_timed_run_timer_cb(lv_timer_t* timer)
{
	int64_t cur_timestamp;
	uint32_t delta_t;
	
	// Evaluate start-of-run_eval_timer
	if ((timer_state != TIMER_STATE_IDLE) && (start_timestamp == 0) && (speed > 0)) {
		start_timestamp = esp_timer_get_time();  // uSec
	}
	
	// Evaluate false start
	if ((timer_state != TIMER_STATE_IDLE) && (timer_state < TIMER_STATE_A3) && (speed > 0)) {
		false_start = true;
	}
	
	// Evaluate timer display update
	if ((timer_state >= TIMER_STATE_TRIGGERED) && (timer_state <= TIMER_STATE_RUNNING2) && (start_timestamp > 0)) {
		cur_timestamp = esp_timer_get_time();
		delta_t = (cur_timestamp - start_timestamp) / 1000;
		_gui_tile_timed_update_timer_display(delta_t);
	}
	
	// Evaluate state
	switch (timer_state) {
		case TIMER_STATE_IDLE:
			// Wait here to be triggered
			break;
		case TIMER_STATE_STARTERR1:
			if (--timer_countdown == 0) {
				_gui_tile_timed_set_timer_state(TIMER_STATE_STARTERR2);
			}
			break;
		case TIMER_STATE_STARTERR2:
			if (--timer_countdown == 0) {
				_gui_tile_timed_set_timer_state(TIMER_STATE_STARTERR3);
			}
			break;
		case TIMER_STATE_STARTERR3:
			if (--timer_countdown == 0) {
				_gui_tile_timed_set_timer_state(TIMER_STATE_IDLE);
			}
			break;
		case TIMER_STATE_TRIGGERED:
			if (--timer_countdown == 0) {
				_gui_tile_timed_set_timer_state(TIMER_STATE_A1);
			}
			break;
		case TIMER_STATE_A1:
			if (--timer_countdown == 0) {
				_gui_tile_timed_set_timer_state(TIMER_STATE_A2);
			}
			break;
		case TIMER_STATE_A2:
			if (--timer_countdown == 0) {
				_gui_tile_timed_set_timer_state(TIMER_STATE_A3);
			}
			break;
		case TIMER_STATE_A3:
			if (--timer_countdown == 0) {
				_gui_tile_timed_set_timer_state(TIMER_STATE_RUNNING1);
			}
			break;
		case TIMER_STATE_RUNNING1:
			if (--timer_countdown == 0) {
				_gui_tile_timed_set_timer_state(TIMER_STATE_RUNNING2);
			}
			break;
		case TIMER_STATE_RUNNING2:
			if (speed >= speed_goal) {
				if (false_start) {
					_gui_tile_timed_set_timer_state(TIMER_STATE_ERROR);
				} else {
					_gui_tile_timed_set_timer_state(TIMER_STATE_DONE);
				}
			} else if (--timer_countdown == 0) {
				// Reset the timer display to 0 to let them know this isn't a valid run if they are just too slow
				_gui_tile_timed_update_timer_display(0);
				_gui_tile_timed_set_timer_state(TIMER_STATE_ERROR);
			}
			break;
		case TIMER_STATE_DONE:
		case TIMER_STATE_ERROR:
			if (--timer_countdown == 0) {
				_gui_tile_timed_set_timer_state(TIMER_STATE_IDLE);
			}
			break;
		default:
			_gui_tile_timed_update_timer_display(0);
			_gui_tile_timed_set_timer_state(TIMER_STATE_IDLE);
	}
}


static void _gui_tile_timed_set_timer_state(int state)
{
	switch (state) {
		case TIMER_STATE_IDLE:
			_gui_tile_timed_update_xmas_tree(XMAS_STATE_OFF);
			break;
		case TIMER_STATE_STARTERR1:
			// Start dual short-beep to indicate they can't start
			timer_countdown = COUNTDOWN_BEEP_MSEC / TIMER_EVAL_MSEC;
			_gui_tile_timed_start_beep(COUNTDOWN_BEEP_MSEC);
			break;
		case TIMER_STATE_STARTERR2:
			timer_countdown = COUNTDOWN_BEEP_MSEC / TIMER_EVAL_MSEC;
			break;
		case TIMER_STATE_STARTERR3:
			timer_countdown = COUNTDOWN_BEEP_MSEC / TIMER_EVAL_MSEC;
			_gui_tile_timed_start_beep(COUNTDOWN_BEEP_MSEC);
			break;
		case TIMER_STATE_TRIGGERED:
			false_start = false;
			timer_countdown = TEST_GO_BEEP_MSEC / TIMER_EVAL_MSEC;
			start_timestamp = 0;   // Set to a non-zero number when first speed detected
			_gui_tile_timed_update_timer_display(0);
			_gui_tile_timed_update_xmas_tree(XMAS_STATE_OFF);
			_gui_tile_timed_start_beep(TEST_GO_BEEP_MSEC);
			break;
		case TIMER_STATE_A1:
			// No beep for A1 since we've just ended a long "start" beep
			timer_countdown = COUNTDOWN_STEP_MSEC / TIMER_EVAL_MSEC;
			_gui_tile_timed_update_xmas_tree(XMAS_STATE_A1);
			break;
		case TIMER_STATE_A2:
			timer_countdown = COUNTDOWN_STEP_MSEC / TIMER_EVAL_MSEC;
			_gui_tile_timed_update_xmas_tree(XMAS_STATE_A2);
			_gui_tile_timed_start_beep(COUNTDOWN_BEEP_MSEC);
			break;
		case TIMER_STATE_A3:
			timer_countdown = COUNTDOWN_STEP_MSEC / TIMER_EVAL_MSEC;
			_gui_tile_timed_update_xmas_tree(XMAS_STATE_A3);
			_gui_tile_timed_start_beep(COUNTDOWN_BEEP_MSEC);
			break;
		case TIMER_STATE_RUNNING1:
			// Green LED on
			timer_countdown = COUNTDOWN_STEP_MSEC / TIMER_EVAL_MSEC;
			_gui_tile_timed_update_xmas_tree(XMAS_STATE_G);
			_gui_tile_timed_start_beep(TEST_GO_BEEP_MSEC);
			break;
		case TIMER_STATE_RUNNING2:
			// Green LED off
			timer_countdown = (TEST_TIMEOUT_MSEC - COUNTDOWN_STEP_MSEC) / TIMER_EVAL_MSEC;
			_gui_tile_timed_update_xmas_tree(XMAS_STATE_OFF);
			break;
		case TIMER_STATE_DONE:
			timer_countdown = COUNTDOWN_DONE_MSEC / TIMER_EVAL_MSEC;
			_gui_tile_timed_update_xmas_tree(XMAS_STATE_G);
			_gui_tile_timed_start_beep(TEST_GO_BEEP_MSEC);
			break;
		case TIMER_STATE_ERROR:
			timer_countdown = COUNTDOWN_DONE_MSEC / TIMER_EVAL_MSEC;
			_gui_tile_timed_update_xmas_tree(XMAS_STATE_R);
			_gui_tile_timed_start_beep(TEST_GO_BEEP_MSEC);
			break;
	}
	
	timer_state = state;
}


static void _gui_tile_timed_speed_cb(float val)
{
	int32_t s;
	
	// Use this update to mark the intervals for the meter update timer
	gui_utility_note_update();
	
	s = round((units_metric) ? val : gui_util_kph_to_mph(val));
	
	if (s != speed) {
		_gui_tile_timed_update_speed_meter(s, false);
		speed = s;
	}
}
