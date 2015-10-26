#include <pebble.h>
#include "main.h"
#include "detail.h"

// The detail window
static Window *s_detail_window;
static ScrollLayer *s_detail_scroll_layer;
static TextLayer *s_detail_text_layer;

static void detail_window_load(Window *window) {
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(window_layer);
	static char text[512];
	char rating_str[32];
	int index = menu_state.user_detail_index;
	GSize max_size;
	RestaurantInformation *ptr = &(search_result.restaurant_info[index]);
	
	APP_LOG(APP_LOG_LEVEL_INFO, "Detail load");

	if(ptr->rating<=5 && ptr->rating>0)
		snprintf(rating_str, sizeof(rating_str), "%s%d %s", detail_rating_text, (int)(ptr->rating), detail_star_text);
	else
		snprintf(rating_str, sizeof(rating_str), "%s%s", detail_rating_text, detail_nodata_text);

	snprintf(text, sizeof(text), "%s\n%s\n%s\n%s\n%s",
		detail_address_text, (ptr->address!=NULL && strlen(ptr->address)>0)?ptr->address:detail_nodata_text,
		detail_phone_text, (ptr->phone!=NULL && strlen(ptr->phone)>0)?ptr->phone:detail_nodata_text,
		rating_str);

	// setup scroll layer
	s_detail_scroll_layer = scroll_layer_create(bounds);
	scroll_layer_set_click_config_onto_window(s_detail_scroll_layer, window);
	// setup text layer
	s_detail_text_layer = text_layer_create(GRect(bounds.origin.x, bounds.origin.y, bounds.size.w, 2000));  // increase size
	text_layer_set_font(s_detail_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
	text_layer_set_text_alignment(s_detail_text_layer, GTextAlignmentLeft);
	text_layer_set_background_color(s_detail_text_layer, bg_color);
	text_layer_set_text_color(s_detail_text_layer, text_color);
	text_layer_set_text(s_detail_text_layer, text);
	max_size = text_layer_get_content_size(s_detail_text_layer);
	max_size.h += 10;  // increase height for Chinese fonts
	text_layer_set_size(s_detail_text_layer, max_size);
	scroll_layer_set_content_size(s_detail_scroll_layer, GSize(bounds.size.w, max_size.h + 10));
	scroll_layer_add_child(s_detail_scroll_layer, text_layer_get_layer(s_detail_text_layer));

	// only need to add scroll layer
	layer_add_child(window_layer, scroll_layer_get_layer(s_detail_scroll_layer));
}

static void detail_window_unload(Window *window) {
	scroll_layer_destroy(s_detail_scroll_layer);
	text_layer_destroy(s_detail_text_layer);

	window_destroy(window);
	s_detail_window = NULL;
}

void detail_window_push(void){
	if(!s_detail_window){
		// Create result window
		s_detail_window = window_create();
		window_set_window_handlers(s_detail_window, (WindowHandlers){
			.load = detail_window_load,
			.unload = detail_window_unload
		});
	}
	window_stack_push(s_detail_window, true);
}
