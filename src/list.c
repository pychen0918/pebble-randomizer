#include <pebble.h>
#include "main.h"
#include "list.h"
#include "detail.h"
#include "wait.h"

// The list menu for all candidate
static Window *s_list_window;
static MenuLayer *s_list_menu_layer;

// sort the restaurants by distance and store the result in sorted_index array
static void list_menu_sort_by_distance(void){
	int i, j;
	uint8_t *a, *b, temp;
	uint8_t num = search_result.num_of_restaurant;
	RestaurantInformation *ptr = &(search_result.restaurant_info[0]);

	reset_sorted_index();
	for(i=0;i<num;i++){
		for(j=i+1;j<num;j++){
			a = &(search_result.sorted_index[i]);
			b = &(search_result.sorted_index[j]);
			if( ptr[*a].distance > ptr[*b].distance){
				temp = *a;
				*a = *b;
				*b = temp; 
			}
		}
	}
}

static uint16_t list_menu_get_num_rows_callback(struct MenuLayer *menulayer, uint16_t section_index, void *callback_context){
	return search_result.num_of_restaurant;
}

static int16_t list_menu_get_header_height_callback(struct MenuLayer *menu_layer, uint16_t section_index, void *callback_context){
	return LIST_MENU_HEADER_HEIGHT;
}

static int16_t list_menu_get_cell_height_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context){
	char *text;
	TextLayer *temp;
	GRect bounds;
	GSize content_size;

	text = search_result.restaurant_info[search_result.sorted_index[cell_index->row]].name;
	bounds = layer_get_bounds(menu_layer_get_layer(menu_layer));
	temp = text_layer_create(GRect(bounds.origin.x+5, bounds.origin.y, bounds.size.w-5, bounds.size.h));
	text_layer_set_font(temp, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
	text_layer_set_overflow_mode(temp, GTextOverflowModeWordWrap);
	text_layer_set_text(temp, text);
	content_size = text_layer_get_content_size(temp);
	text_layer_destroy(temp);

	return content_size.h + 24;
}

static void list_menu_draw_row_handler(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *callback_context){
	int index = search_result.sorted_index[cell_index->row];
	RestaurantInformation *ptr = &(search_result.restaurant_info[index]);
	char *text = ptr->name;
	char sub_text[32];
	GRect bounds = layer_get_bounds(cell_layer);

	if(user_setting.unit == 0)
		snprintf(sub_text, sizeof(sub_text), "%s %u %s", direction_name[ptr->direction], 
			(unsigned int)(ptr->distance), setting_unit_option_text[user_setting.unit]);
	else
		snprintf(sub_text, sizeof(sub_text), "%s %u.%u %s", direction_name[ptr->direction], (unsigned int)(ptr->distance/100), 
			(unsigned int)(ptr->distance%100), setting_unit_option_text[user_setting.unit]);

#ifdef PBL_COLOR
	if(menu_cell_layer_is_highlighted(cell_layer)){
		graphics_context_set_text_color(ctx, highlight_text_color);
		graphics_context_set_fill_color(ctx, highlight_bg_color);
	}
	else{
		graphics_context_set_text_color(ctx, text_color);
		graphics_context_set_fill_color(ctx, bg_color);
	}
#else
	graphics_context_set_text_color(ctx, text_color);
	graphics_context_set_fill_color(ctx, bg_color);
#endif

#ifdef PBL_ROUND
	graphics_draw_text(ctx, text, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
		GRect(bounds.origin.x+5, bounds.origin.y-2, bounds.size.w-5, bounds.size.h),
		GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
	graphics_draw_text(ctx, sub_text, fonts_get_system_font(FONT_KEY_GOTHIC_18),
		GRect(bounds.origin.x+5, bounds.origin.y + bounds.size.h - 22, bounds.size.w, bounds.size.h),
		GTextOverflowModeFill, GTextAlignmentCenter, NULL);
#else
	graphics_draw_text(ctx, text, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
		GRect(bounds.origin.x+5, bounds.origin.y-2, bounds.size.w-5, bounds.size.h),
		GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
	graphics_draw_text(ctx, sub_text, fonts_get_system_font(FONT_KEY_GOTHIC_18),
		GRect(bounds.origin.x+5, bounds.origin.y + bounds.size.h - 22, bounds.size.w, bounds.size.h),
		GTextOverflowModeFill, GTextAlignmentLeft, NULL);
#endif
}

static void list_menu_draw_header_handler(GContext *ctx, const Layer *cell_layer, uint16_t section_index, void *callback_context){
	graphics_context_set_text_color(ctx, highlight_alt_text_color);
	graphics_context_set_fill_color(ctx, highlight_alt_bg_color);
	graphics_fill_rect(ctx, layer_get_bounds(cell_layer), 0, GCornerNone);
#ifdef PBL_ROUND
	graphics_draw_text(ctx, setting_type_option_text[user_setting.type], fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), 
		layer_get_bounds(cell_layer), GTextOverflowModeFill, GTextAlignmentCenter, NULL); 
#else
	graphics_draw_text(ctx, setting_type_option_text[user_setting.type], fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), 
		layer_get_bounds(cell_layer), GTextOverflowModeFill, GTextAlignmentLeft, NULL); 
#endif
}

static void list_menu_select_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context){
	// index is the "real index" that map to actual restaurant data
	// we need to map cell_index back to real index to acquire real detail data
	int index = search_result.sorted_index[cell_index->row];

	menu_state.user_operation = USER_OPERATION_DETAIL;
	menu_state.user_detail_index = index;

	// Check if we have detail information already
	if(search_result.restaurant_info[index].address != NULL){
		// show detail window
		detail_window_push();
	}
	else{
		send_detail_query((uint8_t)index);
		wait_window_push();
	}
}

static void list_window_load(Window *window) {
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(window_layer);

	APP_LOG(APP_LOG_LEVEL_INFO, "List load");
	list_menu_sort_by_distance();

	s_list_menu_layer = menu_layer_create(bounds);
#ifdef PBL_COLOR
	menu_layer_set_normal_colors(s_list_menu_layer, bg_color, text_color);
	menu_layer_set_highlight_colors(s_list_menu_layer, highlight_bg_color, highlight_text_color);
#endif	
	menu_layer_set_callbacks(s_list_menu_layer, NULL, (MenuLayerCallbacks){
		.get_num_rows = list_menu_get_num_rows_callback,
		.get_cell_height = list_menu_get_cell_height_callback,
		.draw_row = list_menu_draw_row_handler,
		.select_click = list_menu_select_callback,
		.get_header_height = list_menu_get_header_height_callback,
		.draw_header = list_menu_draw_header_handler
	});
	menu_layer_set_click_config_onto_window(s_list_menu_layer, window);
	layer_add_child(window_layer, menu_layer_get_layer(s_list_menu_layer));
}

static void list_window_unload(Window *window) {
	menu_layer_destroy(s_list_menu_layer);

	window_destroy(window);
	s_list_window = NULL;
}

void list_window_push(void){
	if(!s_list_window){
		// Create list window
		s_list_window = window_create();
		window_set_window_handlers(s_list_window, (WindowHandlers){
			.load = list_window_load,
			.unload = list_window_unload
		});
	}
	window_stack_push(s_list_window, true);
}
