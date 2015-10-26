#include <pebble.h>
#include "main.h"
#include "setting.h"

// The settings window
static Window *s_setting_window;
static Window *s_setting_sub_window;
static MenuLayer *s_setting_main_menu_layer;
static MenuLayer *s_setting_sub_menu_layer;

static void setting_sub_window_push(void);

static uint16_t setting_main_menu_get_num_rows_callback(struct MenuLayer *menulayer, uint16_t section_index, void *callback_context){
	return sizeof(setting_main_menu_text)/sizeof(setting_main_menu_text[0]);
}

static int16_t setting_main_menu_get_header_height_callback(struct MenuLayer *menu_layer, uint16_t section_index, void *callback_context){
	return SETTING_MENU_HEADER_HEIGHT;
}

static void setting_main_menu_draw_row_handler(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *callback_context){
	const char *text = setting_main_menu_text[cell_index->row];
	const char *sub_text = NULL;
	
	switch(cell_index->row){
		case SETTING_MENU_OPTION_RANGE:
			sub_text = setting_range_option_text[user_setting.range];
			break;
		case SETTING_MENU_OPTION_TYPE:
			sub_text = setting_type_option_text[user_setting.type];
			break;
		case SETTING_MENU_OPTION_OPENNOW:
			sub_text = setting_opennow_option_text[user_setting.opennow];
			break;
		default:
			break;
	}

	menu_cell_basic_draw(ctx, cell_layer, text, sub_text, NULL);
}

static void setting_main_menu_draw_header_handler(GContext *ctx, const Layer *cell_layer, uint16_t section_index, void *callback_context){
	graphics_context_set_text_color(ctx, highlight_alt_text_color);
	graphics_context_set_fill_color(ctx, highlight_alt_bg_color);
	graphics_fill_rect(ctx, layer_get_bounds(cell_layer), 0, GCornerNone);
#ifdef PBL_ROUND
	graphics_draw_text(ctx, setting_main_menu_header_text, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), 
		layer_get_bounds(cell_layer), GTextOverflowModeFill, GTextAlignmentCenter, NULL); 
#else
	graphics_draw_text(ctx, setting_main_menu_header_text, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), 
		layer_get_bounds(cell_layer), GTextOverflowModeFill, GTextAlignmentLeft, NULL); 
#endif
}

static void setting_main_menu_select_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context){
	menu_state.setting_menu_selected_option = cell_index->row;

	setting_sub_window_push();
}

static void setting_window_load(Window *window){
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(window_layer);

	APP_LOG(APP_LOG_LEVEL_INFO, "Settings load");

	icon_blank_bitmap = gbitmap_create_with_resource(RESOURCE_ID_ICON_BLANK);
	icon_check_black_bitmap = gbitmap_create_with_resource(RESOURCE_ID_ICON_CHECK_BLACK);
	s_setting_main_menu_layer = menu_layer_create(bounds);
#ifdef PBL_COLOR
	menu_layer_set_normal_colors(s_setting_main_menu_layer, bg_color, text_color);
	menu_layer_set_highlight_colors(s_setting_main_menu_layer, highlight_bg_color, highlight_text_color);
#endif
	menu_layer_set_callbacks(s_setting_main_menu_layer, NULL, (MenuLayerCallbacks){
		.get_num_rows = setting_main_menu_get_num_rows_callback,
		.draw_row = setting_main_menu_draw_row_handler,
		.select_click = setting_main_menu_select_callback,
		.get_header_height = setting_main_menu_get_header_height_callback,
		.draw_header = setting_main_menu_draw_header_handler
	});
	menu_layer_set_click_config_onto_window(s_setting_main_menu_layer, window);
	layer_add_child(window_layer, menu_layer_get_layer(s_setting_main_menu_layer));
}

static void setting_window_unload(Window *window){
	gbitmap_destroy(icon_blank_bitmap);
	gbitmap_destroy(icon_check_black_bitmap);
	menu_layer_destroy(s_setting_main_menu_layer);
	window_destroy(window);
	s_setting_window = NULL;
}

void setting_window_push(void){
	if(!s_setting_window){
		// Create settings window
		s_setting_window = window_create();
		window_set_window_handlers(s_setting_window, (WindowHandlers){
			.load = setting_window_load,
			.unload = setting_window_unload
		});
	}
	window_stack_push(s_setting_window, true);
}

static uint16_t setting_sub_menu_get_num_rows_callback(struct MenuLayer *menulayer, uint16_t section_index, void *callback_context){
	uint16_t ret = 0;
	switch(menu_state.setting_menu_selected_option){
		case SETTING_MENU_OPTION_RANGE:
			ret = sizeof(setting_range_option_text)/sizeof(setting_range_option_text[0]);
			break;
		case SETTING_MENU_OPTION_TYPE:
			ret = sizeof(setting_type_option_text)/sizeof(setting_type_option_text[0]);
			break;
		case SETTING_MENU_OPTION_OPENNOW:
			ret = sizeof(setting_opennow_option_text)/sizeof(setting_opennow_option_text[0]);
			break;
		default:
			break;
	}
	return ret;
}

static void setting_sub_menu_draw_row_handler(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *callback_context){
	const char *text = NULL;
	uint8_t selected_index = 0;  // the one that is selected previously by user

	switch(menu_state.setting_menu_selected_option){
		case SETTING_MENU_OPTION_RANGE:
			selected_index = user_setting.range;
			text = setting_range_option_text[cell_index->row];
			break;
		case SETTING_MENU_OPTION_TYPE:
			selected_index = user_setting.type;
			text = setting_type_option_text[cell_index->row];
			break;
		case SETTING_MENU_OPTION_OPENNOW:
			selected_index = user_setting.opennow;
			text = setting_opennow_option_text[cell_index->row];
			break;
		default:
			break;
	}

#ifdef PBL_ROUND
	if(cell_index->row == selected_index)
		menu_cell_basic_draw(ctx, cell_layer, text, NULL, icon_check_black_bitmap);
	else
		menu_cell_basic_draw(ctx, cell_layer, text, NULL, NULL);
#else
	if(cell_index->row == selected_index)
		menu_cell_basic_draw(ctx, cell_layer, text, NULL, icon_check_black_bitmap);
	else
		menu_cell_basic_draw(ctx, cell_layer, text, NULL, icon_blank_bitmap);
#endif
}

static void setting_sub_menu_select_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context){
	UserSetting old;

	memcpy(&old, &user_setting, sizeof(old));

	switch(menu_state.setting_menu_selected_option){
		case SETTING_MENU_OPTION_RANGE:
			user_setting.range = cell_index->row;
			break;
		case SETTING_MENU_OPTION_TYPE:
			user_setting.type = cell_index->row;
			break;
		case SETTING_MENU_OPTION_OPENNOW:
			user_setting.opennow = cell_index->row;
			break;
		default:
			break;
	}
	if(memcmp(&old, &user_setting, sizeof(old))){
		APP_LOG(APP_LOG_LEVEL_DEBUG, "Settings changed");
		search_result.is_setting_changed = true;
	}
	else
		search_result.is_setting_changed = false;

	// Once an item is selected, close this sub_menu window
	window_stack_pop(true);
}

static void setting_sub_window_load(Window *window){
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(window_layer);

	APP_LOG(APP_LOG_LEVEL_INFO, "Settings sub window load");
	
	s_setting_sub_menu_layer = menu_layer_create(bounds);
	menu_layer_set_callbacks(s_setting_sub_menu_layer, NULL, (MenuLayerCallbacks){
		.get_num_rows = setting_sub_menu_get_num_rows_callback,
		.draw_row = setting_sub_menu_draw_row_handler,
		.select_click = setting_sub_menu_select_callback,
	});
#ifdef PBL_COLOR
	menu_layer_set_highlight_colors(s_setting_sub_menu_layer, highlight_bg_color, highlight_text_color);
#endif
	menu_layer_set_click_config_onto_window(s_setting_sub_menu_layer, window);
	layer_add_child(window_layer, menu_layer_get_layer(s_setting_sub_menu_layer));
}

static void setting_sub_window_unload(Window *window){
	menu_layer_destroy(s_setting_sub_menu_layer);

	window_destroy(window);
	s_setting_sub_window = NULL;
}

static void setting_sub_window_push(void){
	if(!s_setting_sub_window){
		// Create settings sub menu window
		s_setting_sub_window = window_create();
		window_set_window_handlers(s_setting_sub_window, (WindowHandlers){
			.load = setting_sub_window_load,
			.unload = setting_sub_window_unload
		});
	}
	window_stack_push(s_setting_sub_window, true);
}
