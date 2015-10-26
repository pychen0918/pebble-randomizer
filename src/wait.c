#include <pebble.h>
#include "main.h"
#include "wait.h"
#include "query.h"
#include "result.h"

static AppTimer *s_wait_animation_timer;// Timer for waiting animation
static int s_wait_animation_counter = 0;
static AppTimer *s_wait_timeout_timer;	// Timer for dismissing waiting window

// The waiting window
static Window *s_wait_window;
static TextLayer *s_wait_text_layer;
static Layer *s_wait_layer;

static void wait_timeout_timer_callback(void *context);
static void wait_animation_timer_callback(void *context);
static void wait_animation_next_timer(void);

static void wait_timeout_timer_callback(void *context){
	// Forge result and call result window
	search_result.is_querying = false;
	search_result.query_status = QUERY_STATUS_GPS_TIMEOUT;
	result_window_push();
	window_stack_remove(s_wait_window, false);
}

static void wait_animation_timer_callback(void *context){
	s_wait_animation_counter += (s_wait_animation_counter < 100) ? 1 : -100;
	layer_mark_dirty(s_wait_layer);
	wait_animation_next_timer();
}

static void wait_animation_next_timer(void){
	s_wait_animation_timer = app_timer_register(WAIT_ANIMATION_TIMER_DELTA, wait_animation_timer_callback, NULL);
}

static void wait_layer_update_proc(Layer *layer, GContext *ctx){
	GRect bounds = layer_get_bounds(layer);
	int bar_max_length = (bounds.size.w - WAIT_ANIMATION_BAR_LEFT_MARGIN - WAIT_ANIMATION_BAR_RIGHT_MARGIN);
	int y_pos = (bounds.size.h / 2);

	if(bar_max_length < 0){
		bar_max_length = bounds.size.w;
	}

	int width = (int)(float)(((float)s_wait_animation_counter / 100.0F) * bar_max_length);
#ifdef PBL_COLOR
	graphics_context_set_stroke_color(ctx, highlight_text_color);
	graphics_draw_round_rect(ctx, GRect(WAIT_ANIMATION_BAR_LEFT_MARGIN, y_pos, width, WAIT_ANIMATION_BAR_HEIGHT), WAIT_ANIMATION_BAR_RADIUS);
	graphics_context_set_fill_color(ctx, highlight_bg_color);
#else
	graphics_context_set_fill_color(ctx, GColorBlack);
#endif
	graphics_fill_rect(ctx, GRect(WAIT_ANIMATION_BAR_LEFT_MARGIN, y_pos, width, WAIT_ANIMATION_BAR_HEIGHT), 
			   WAIT_ANIMATION_BAR_RADIUS, GCornersAll);
}

static void wait_window_load(Window *window) {
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(window_layer);

	APP_LOG(APP_LOG_LEVEL_INFO, "Wait load");

	s_wait_text_layer = text_layer_create(GRect(bounds.origin.x, bounds.origin.y, bounds.size.w, WAIT_TEXT_LAYER_HEIGHT));
	s_wait_layer = layer_create(GRect(bounds.origin.x, bounds.origin.y + WAIT_TEXT_LAYER_HEIGHT, 
					  bounds.size.w, bounds.size.w - WAIT_TEXT_LAYER_HEIGHT));
	
	text_layer_set_font(s_wait_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
	text_layer_set_text_alignment(s_wait_text_layer, GTextAlignmentCenter);
	text_layer_set_background_color(s_wait_text_layer, highlight_alt_bg_color);
	text_layer_set_text_color(s_wait_text_layer, highlight_alt_text_color);
	text_layer_set_text(s_wait_text_layer, wait_layer_header_text);
	layer_set_update_proc(s_wait_layer, wait_layer_update_proc);

	layer_add_child(window_layer, text_layer_get_layer(s_wait_text_layer));
	layer_add_child(window_layer, s_wait_layer);
}

static void wait_window_unload(Window *window) {
	layer_destroy(s_wait_layer);
	text_layer_destroy(s_wait_text_layer);

	window_destroy(window);
	s_wait_window = NULL;
}

static void wait_window_appear(Window *window) {
	s_wait_animation_counter = 0;
	wait_animation_next_timer();
	s_wait_timeout_timer = app_timer_register(WAIT_WINDOW_TIMEOUT, wait_timeout_timer_callback, NULL); 
}

static void wait_window_disappear(Window *window) {
	if(s_wait_animation_timer){
		app_timer_cancel(s_wait_animation_timer);
		s_wait_animation_timer = NULL;
	}
	if(s_wait_timeout_timer){
		app_timer_cancel(s_wait_timeout_timer);
		s_wait_timeout_timer = NULL;
	}
}

void wait_window_push(void){
	if(!s_wait_window){
		// Create wait window
		s_wait_window = window_create();
		window_set_window_handlers(s_wait_window, (WindowHandlers){
			.appear = wait_window_appear,
			.load = wait_window_load,
			.unload = wait_window_unload,
			.disappear = wait_window_disappear
		});
	}
	window_stack_push(s_wait_window, true);
}

Window *get_wait_window(void){
	return s_wait_window;
}
