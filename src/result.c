#include <pebble.h>
#include "main.h"
#include "result.h"
#include "detail.h"
#include "wait.h"

// The random pick result
static Window *s_result_window;
static TextLayer *s_result_title_text_layer;
static TextLayer *s_result_sub_text_layer;
static ActionBarLayer *s_result_action_bar_layer;
static ScrollLayer *s_result_scroll_layer;

static void result_select_click_handler(ClickRecognizerRef recognizer, void *context) {
	menu_state.user_detail_index = search_result.random_result;
	menu_state.user_operation = USER_OPERATION_DETAIL;

	// Check if we have detail information already
	if(search_result.restaurant_info[menu_state.user_detail_index].address != NULL){
		// show detail window
		detail_window_push();
	}
	else{
		send_detail_query((uint8_t)(menu_state.user_detail_index));
		wait_window_push();
	}
}

static void result_click_config_provider(void *context){
	window_single_click_subscribe(BUTTON_ID_SELECT, (ClickHandler)result_select_click_handler);
}

static void result_window_load(Window *window) {
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(window_layer);
	static char title_text[256], sub_text[256];
	RestaurantInformation *ptr;
	uint8_t status;
	int text_layer_width;
	bool valid_result = false; // create action bar and bind key pressing only when valid result is displayed
	GSize max_size, sub_max_size;
	int title_height;
	TextLayer *temp;
	
	APP_LOG(APP_LOG_LEVEL_DEBUG, "Result window load");
	status = search_result.query_status;
	switch(status){
		case QUERY_STATUS_SUCCESS:
			valid_result = true;
			// Randomly pick up the restaurant
			search_result.random_result = rand()%search_result.num_of_restaurant;
			// Collect required fields
			ptr = &(search_result.restaurant_info[search_result.random_result]);
			strncpy(title_text, ptr->name, sizeof(title_text));
			snprintf(sub_text, sizeof(sub_text), "%s %u %s", direction_name[ptr->direction], (unsigned int)(ptr->distance), distance_unit);
			break;
		case QUERY_STATUS_NO_RESULT:
		case QUERY_STATUS_GPS_TIMEOUT:
			// Set corresponding error messages to title and subtitle
			strncpy(title_text, query_status_error_message[status], sizeof(title_text));
			strncpy(sub_text, query_status_error_sub_message[status], sizeof(sub_text));
			break;
		case QUERY_STATUS_GOOGLE_API_ERROR:
			// Collect error message from returned information
			strncpy(title_text, query_status_error_message[status], sizeof(title_text));
			strncpy(sub_text, search_result.api_error_message, sizeof(sub_text));
			break;
		default:
			// Other query status = unknown error
			strncpy(title_text, unknown_error_message, sizeof(title_text));
			snprintf(sub_text, sizeof(sub_text), "%s %s%d", unknown_error_sub_message, "Incorrect query status:",status);
			break;
	}
	
	// action bar part
	if(valid_result == true){
		icon_agenda_bitmap = gbitmap_create_with_resource(RESOURCE_ID_ICON_AGENDA);
		text_layer_width = bounds.size.w - ACTION_BAR_WIDTH;
		s_result_action_bar_layer = action_bar_layer_create();
#ifdef PBL_PLATFORM_BASALT
		action_bar_layer_set_background_color(s_result_action_bar_layer, highlight_alt_bg_color);
#endif
		action_bar_layer_add_to_window(s_result_action_bar_layer, window);
		action_bar_layer_set_icon(s_result_action_bar_layer, BUTTON_ID_SELECT, icon_agenda_bitmap);
	}
	else{
		text_layer_width = bounds.size.w;
	}
	// compute require height for sub-title
	temp = text_layer_create(GRect(bounds.origin.x, bounds.origin.y, text_layer_width, bounds.size.h));
	text_layer_set_font(temp, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
	text_layer_set_text(temp, sub_text);
	sub_max_size = text_layer_get_content_size(temp);
	text_layer_destroy(temp);

	// title part
	s_result_title_text_layer = text_layer_create(GRect(bounds.origin.x, bounds.origin.y, text_layer_width, 2000));
	text_layer_set_font(s_result_title_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
	text_layer_set_text_alignment(s_result_title_text_layer, GTextAlignmentLeft);
	text_layer_set_overflow_mode(s_result_title_text_layer, GTextOverflowModeWordWrap);
	text_layer_set_background_color(s_result_title_text_layer, bg_color);
	text_layer_set_text_color(s_result_title_text_layer, text_color);
	text_layer_set_text(s_result_title_text_layer, title_text);
	max_size = text_layer_get_content_size(s_result_title_text_layer);

	// adjust title height. Always have 10px padding for certain characters to display
	if(max_size.h > (bounds.size.h - sub_max_size.h + 10))
		title_height = (bounds.size.h - sub_max_size.h + 10);  // title is very long: leave some space for sub-title.
	else if(max_size.h < (bounds.size.h/2 + 10))
		title_height = (bounds.size.h/2 + 10);  // title is short: make it half part
	else
		title_height = max_size.h + 10;  // title is long, but not very long: use current size

	// create scroll layer
	s_result_scroll_layer = scroll_layer_create(GRect(bounds.origin.x, bounds.origin.y, text_layer_width, title_height));
	if(valid_result == true){
		scroll_layer_set_click_config_onto_window(s_result_scroll_layer, window);
		scroll_layer_set_callbacks(s_result_scroll_layer, (ScrollLayerCallbacks){.click_config_provider=result_click_config_provider});
	}
	scroll_layer_set_content_size(s_result_scroll_layer, GSize(text_layer_width, max_size.h));
	scroll_layer_add_child(s_result_scroll_layer, text_layer_get_layer(s_result_title_text_layer));
	layer_add_child(window_layer, scroll_layer_get_layer(s_result_scroll_layer));

	// sub title part
	s_result_sub_text_layer = text_layer_create(GRect(bounds.origin.x, bounds.origin.y+title_height, text_layer_width, (bounds.size.h-title_height)));
	text_layer_set_font(s_result_sub_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
	text_layer_set_text_alignment(s_result_sub_text_layer, GTextAlignmentLeft);
	text_layer_set_overflow_mode(s_result_sub_text_layer, GTextOverflowModeFill);
	text_layer_set_background_color(s_result_sub_text_layer, highlight_bg_color);
	text_layer_set_text_color(s_result_sub_text_layer, highlight_text_color);
	text_layer_set_text(s_result_sub_text_layer, sub_text);
	layer_add_child(window_layer, text_layer_get_layer(s_result_sub_text_layer));
}

static void result_window_unload(Window *window) {
	APP_LOG(APP_LOG_LEVEL_DEBUG, "Result window unload");

	text_layer_destroy(s_result_title_text_layer);
	text_layer_destroy(s_result_sub_text_layer);
	action_bar_layer_destroy(s_result_action_bar_layer);
	scroll_layer_destroy(s_result_scroll_layer);
	if(icon_agenda_bitmap != NULL){
		gbitmap_destroy(icon_agenda_bitmap);
		icon_agenda_bitmap = NULL;
	}

	window_destroy(window);
	s_result_window = NULL;
}

void result_window_push(void){
	APP_LOG(APP_LOG_LEVEL_DEBUG, "Result window push");
	if(!s_result_window){
		APP_LOG(APP_LOG_LEVEL_DEBUG, "Create new result window");
		// Create result window
		s_result_window = window_create();
		window_set_window_handlers(s_result_window, (WindowHandlers){
			.load = result_window_load,
			.unload = result_window_unload
		});
	}
	APP_LOG(APP_LOG_LEVEL_DEBUG, "About to push result window to stack");
	window_stack_push(s_result_window, true);
}
