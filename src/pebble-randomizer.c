#include <pebble.h>
#define MAX_DATA_NUMBER 20 		// Max number of returned data
#define MAIN_MENU_ROWS	3  		// Random, list and settings
#define LIST_MENU_ROWS  MAX_DATA_NUMBER
#define RESULT_AGE_TIME	300 		// How long the list is valid in seconds
#define SELECT_OPTION_RANDOM	0
#define SELECT_OPTION_LIST	1
#define MAIN_MENU_TEXT_LENGTH	64
#define LIST_MENU_TEXT_LENGTH	128
#define LIST_MENU_SUB_TEXT_LENGTH	16
#define LIST_MENU_HEADER_HEIGHT 	18
#define SETTING_MENU_TEXT_LENGTH	64
#define SETTING_MENU_SUB_TEXT_LENGTH	64
#define SETTING_MENU_HEADER_HEIGHT	18
#define WAIT_TEXT_LAYER_HEIGHT		32
#define WAIT_ANIMATION_TIMER_DELTA	33
#define WAIT_ANIMATION_BAR_LEFT_MARGIN	10
#define WAIT_ANIMATION_BAR_RIGHT_MARGIN	10
#define WAIT_ANIMATION_BAR_HEIGHT	6
#define WAIT_ANIMATION_BAR_RADIUS	(WAIT_ANIMATION_BAR_HEIGHT/2)

#define PERSIST_KEY_SEARCH_DATA		40
#define DEFAULT_SEARCH_RADIUS		1	// 1km
#define DEFAULT_SEARCH_TYPE		0	// Restaurant
#define DEFAULT_SEARCH_OPENNOW		0	// do not add opennow filter
#define KEY_SEARCH_OPTION		99	// key for appmessage

// --------------------------------------------------------------------------------------
// XXX: Data Structure Defination
// --------------------------------------------------------------------------------------

typedef struct __restaurant_information_t{
	char *name;		// name of the restaurant
	char *place_id;		// the Google place_id. Need for searching telphone number
	char *address;		// the short address
	char *tel;		// the telphone number
	uint8_t rating;		// user rating of the store. Interger between 0 ~ 5
	uint8_t direction;	// compass direction from current loation, e.g., N, NE, E, ...
	uint16_t distance;	// distance between current location and the restraunt. Roundup to 10 meters.
} RestaurantInformation;

typedef struct __search_result_t{
	RestaurantInformation restaurant_info[MAX_DATA_NUMBER];	// The above restaurant information. Up to 20 restaurants.
	time_t last_query_time;		// The epoch time of the last successful query. Use to check if the data is valid or not.
	uint8_t num_of_restaurant;	// How many restaurants actually found
	uint8_t query_status;		// Record currect status. Use to check if the data is valid or not.
	uint8_t random_result;		// Record the index of random picked restaurant. Between 0 ~ (num_of_restaurant-1)
	bool is_querying;		// Are we currently waiting for query result returns. Do not send another one.
} SearchResult;

typedef struct __user_setting_t{
	uint8_t range_index;	// Index of selected range (0: 500m, 1: 1km, 2: 5km, 3: 10km)
	uint8_t keyword_index;	// Index of selected keyword (0: Restaurant, 1: Food)
	uint8_t opennow_index;	// Index of selected opennow filter (0: Disable, 1: Enable)
	bool is_setting_changed;// if user has made changes after last query
} UserSetting;

typedef struct __menu_status_t{
	UserSetting user_setting;		// The above user configuations.
	uint8_t main_menu_selected_option; 	// which function is selected by user (Random, List, Setting)
	uint8_t setting_menu_selected_option; 	// which setting menu is selected by user (Range, Keyword, Open Now)
} MenuState;

// --------------------------------------------------------------------------------------
// XXX: The constant strings
// --------------------------------------------------------------------------------------

const char main_menu_text[][MAIN_MENU_TEXT_LENGTH] = {"Random!", "List", "Settings"};
const char *list_menu_header_text = "Restaurants";
const char *setting_main_menu_header_text = "Options";
const char setting_main_menu_text[][SETTING_MENU_TEXT_LENGTH] = {"Range", "Keyword", "Open Now"};

const char search_distance_display_text[][LIST_MENU_SUB_TEXT_LENGTH] = {"500 M", "1 KM", "5 KM", "10 KM"};
const char search_type_display_text[][LIST_MENU_SUB_TEXT_LENGTH] = {"Restaurants", "Foods"};
const char search_opennow_display_text[][LIST_MENU_SUB_TEXT_LENGTH] = {"No", "Yes"};

// the following string are send in AppMessage for the phone to use in the query
const char search_distance_query_text[][LIST_MENU_SUB_TEXT_LENGTH] = {"500", "1000", "5000", "10000"};
const char search_type_query_text[][LIST_MENU_SUB_TEXT_LENGTH] = {"restaurant", "food"};
const char search_opennow_query_text[][LIST_MENU_SUB_TEXT_LENGTH] = {"", "opennow"};

// --------------------------------------------------------------------------------------
// XXX: The global variables
// --------------------------------------------------------------------------------------

static SearchResult s_search_result;	// The main data structure to store the returned search results
static MenuState s_menu_state;		// The main data structure to store the user's selection on the menu

static AppTimer *s_wait_animation_timer;// Timers for waiting animation
static int s_wait_animation_counter = 0;

// The main menu with "Random" and "List" options
static Window *s_main_window;
static MenuLayer *s_main_menu_layer;

// The random pick result
static Window *s_result_window;
static TextLayer *s_result_text_layer;

// The list menu for all candidate
static Window *s_list_window;
static MenuLayer *s_list_menu_layer;

// The settings window
static Window *s_setting_window;
static Window *s_setting_sub_window;
static MenuLayer *s_setting_main_menu_layer;
static MenuLayer *s_setting_sub_menu_layer;

// The waiting window
static Window *s_wait_window;
static TextLayer *s_wait_text_layer;
static Layer *s_wait_layer;

// TODO:
// Check if current stored restaurant data is valid.
// If not, return false so we can try to send another query.
// Conditions are: query_status, num_of_restaurant, last_query_time, is_settings_changed
static bool validate_data(void){
	return false;
}

static uint32_t compute_search_data(uint8_t radius, uint8_t type, uint8_t opennow){
	return (radius << 16) | (type << 8) | opennow;
}

static void get_search_options(uint32_t search_data, uint8_t *radius, uint8_t *type, uint8_t *opennow){
	*radius = (search_data & 0xff0000) >> 16;
	*type = (search_data & 0xff00) >> 8;
	*opennow = search_data & 0xff;
	APP_LOG(APP_LOG_LEVEL_DEBUG, "radius=%d type=%d opennow=%d", (int)*radius, (int)*type, (int)*opennow);
}

static void send_query(void){
	DictionaryIterator *iter;
	app_message_outbox_begin(&iter);
	APP_LOG(APP_LOG_LEVEL_DEBUG, "search_data=%u", (unsigned int)s_search_data);
	dict_write_uint32(iter, KEY_SEARCH_OPTION, s_search_data);
	//dict_write_uint8(iter, 0, 0);
	app_message_outbox_send();
}

// A wrapped subroutine to determine the query status
// and generate result from list, or error message if query failed
static void generate_random_result_window(void){
	int index = 0;

	// Determine if we have valid query result
	if(!strncmp(query_result, "Success", 7)){
		index = rand()%num_of_list_items;
		strncpy(random_result, list_menu_text[index], sizeof(random_result));
	}
	else{
		if(strlen(query_result)>0)
			strncpy(random_result, query_result, sizeof(random_result));
		else
			strncpy(random_result, "No result!", sizeof(random_result));
	}
	APP_LOG(APP_LOG_LEVEL_DEBUG, "Random result: %d %s", index, random_result);
	window_stack_push(s_result_window, true);
}

static uint16_t main_menu_get_num_rows_callback(struct MenuLayer *menulayer, uint16_t section_index, void *callback_context){
	return MAIN_MENU_ROWS;
}

static void main_menu_draw_row_handler(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *callback_context){
	char* text = main_menu_text[cell_index->row];

	menu_cell_basic_draw(ctx, cell_layer, text, NULL, NULL);
}

static void main_menu_select_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context){
	time_t now;
	bool valid_result = false;

	APP_LOG(APP_LOG_LEVEL_INFO, "Select Click");

	select_option = cell_index->row;
	// Do we have the valid result?
	now = time(NULL);
	if(last_query_time > 0 && ((now - last_query_time) < RESULT_AGE_TIME) && !is_setting_changed){
		valid_result = true;
	}

	switch(cell_index->row){
		case 0:  // Random
			if(valid_result)
				generate_random_result_window();
			else{
				if(is_querying == false)
					send_query();
				window_stack_push(s_wait_window, true);
			}
			break;
		case 1:  // List
			if(valid_result)
				window_stack_push(s_list_window, true);
			else{
				if(is_querying == false)
					send_query();
				window_stack_push(s_wait_window, true);
			}
			break;
		case 2:  // Setting
			window_stack_push(s_setting_window, true);
			break;
		default:
			APP_LOG(APP_LOG_LEVEL_ERROR, "Unknown selection");
			break;
	}
		
}

static void main_window_load(Window *window) {
	// Create Window's child Layers here
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(window_layer);

	APP_LOG(APP_LOG_LEVEL_INFO, "Menu load");

	s_main_menu_layer = menu_layer_create(bounds);
	menu_layer_set_callbacks(s_main_menu_layer, NULL, (MenuLayerCallbacks){
		.get_num_rows = main_menu_get_num_rows_callback,
		.draw_row = main_menu_draw_row_handler,
		.select_click = main_menu_select_callback
	});
	menu_layer_set_click_config_onto_window(s_main_menu_layer, window);
	layer_add_child(window_layer, menu_layer_get_layer(s_main_menu_layer));
}

static void main_window_unload(Window *window) {
	menu_layer_destroy(s_main_menu_layer);
}

static uint16_t list_menu_get_num_rows_callback(struct MenuLayer *menulayer, uint16_t section_index, void *callback_context){
	return num_of_list_items;
}

static int16_t list_menu_get_header_height_callback(struct MenuLayer *menu_layer, uint16_t section_index, void *callback_context){
	return LIST_MENU_HEADER_HEIGHT;
}

static void list_menu_draw_row_handler(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *callback_context){
	char *text = list_menu_text[cell_index->row];
	char *sub_text = list_menu_sub_text[cell_index->row];

	menu_cell_basic_draw(ctx, cell_layer, text, sub_text, NULL);
}

static void list_menu_draw_header_handler(GContext *ctx, const Layer *cell_layer, uint16_t section_index, void *callback_context){
	menu_cell_basic_header_draw(ctx, cell_layer, list_menu_header_text);
}

static void list_menu_select_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context){
// Currently do nothing. Consider to provide detailed information in the future.
	APP_LOG(APP_LOG_LEVEL_INFO, "%d item selected", cell_index->row);
}

static void list_window_load(Window *window) {
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(window_layer);

	APP_LOG(APP_LOG_LEVEL_INFO, "List load");

	s_list_menu_layer = menu_layer_create(bounds);
	menu_layer_set_callbacks(s_list_menu_layer, NULL, (MenuLayerCallbacks){
		.get_num_rows = list_menu_get_num_rows_callback,
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

static void list_window_push(void){
	if(!s_list_window){
		// Create list window
		s_list_window = window_create();
		window_set_window_handlers(s_list_window, (WindowHandlers){
			.load = list_window_load,
			.unload = list_window_unload
		});
	}
}

static void result_window_load(Window *window) {
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(window_layer);
	
	APP_LOG(APP_LOG_LEVEL_INFO, "Result load");

	s_result_text_layer = text_layer_create(bounds);
	text_layer_set_font(s_result_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
	text_layer_set_text_alignment(s_result_text_layer, GTextAlignmentLeft);
	text_layer_set_background_color(s_result_text_layer, GColorClear);
	text_layer_set_text_color(s_result_text_layer, GColorBlack);
	text_layer_set_text(s_result_text_layer, random_result);
	layer_add_child(window_layer, text_layer_get_layer(s_result_text_layer));
}

static void result_window_unload(Window *window) {
	text_layer_destroy(s_result_text_layer);

	window_destroy(window);
	s_result_window = NULL;
}

static void result_window_push(void){
	if(!s_result_window){
		// Create result window
		s_result_window = window_create();
		window_set_window_handlers(s_result_window, (WindowHandlers){
			.load = result_window_load,
			.unload = result_window_unload
		});
	}
}

static uint16_t setting_main_menu_get_num_rows_callback(struct MenuLayer *menulayer, uint16_t section_index, void *callback_context){
	return sizeof(setting_main_menu_text)/sizeof(setting_main_menu_text[0]);
}

static int16_t setting_main_menu_get_header_height_callback(struct MenuLayer *menu_layer, uint16_t section_index, void *callback_context){
	return SETTING_MENU_HEADER_HEIGHT;
}

static void setting_main_menu_draw_row_handler(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *callback_context){
	char *text = setting_main_menu_text[cell_index->row];
	char *sub_text = NULL;
	uint8_t radius, type, opennow;
	get_search_options(s_search_data, &radius, &type, &opennow);
	
	switch(cell_index->row){
		case 0:  // distance
			sub_text = search_distance_display_text[radius];
			break;
		case 1:  // type
			sub_text = search_type_display_text[type];
			break;
		case 2:  // opennow
			sub_text = search_opennow_display_text[opennow];
			break;
		default:
			break;
	}

	menu_cell_basic_draw(ctx, cell_layer, text, sub_text, NULL);
}

static void setting_main_menu_draw_header_handler(GContext *ctx, const Layer *cell_layer, uint16_t section_index, void *callback_context){
	menu_cell_basic_header_draw(ctx, cell_layer, setting_main_menu_header_text);
}

static void setting_main_menu_select_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context){
	select_setting_option = cell_index->row;

	window_stack_push(s_setting_sub_window, true);
}

static void setting_window_load(Window *window){
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(window_layer);

	APP_LOG(APP_LOG_LEVEL_INFO, "Settings load");
	
	s_setting_main_menu_layer = menu_layer_create(bounds);
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
	menu_layer_destroy(s_setting_main_menu_layer);

	window_destroy(window);
	s_setting_window = NULL;
}

static void setting_window_push(void){
	if(!s_setting_window){
		// Create settings window
		s_setting_window = window_create();
		window_set_window_handlers(s_setting_window, (WindowHandlers){
			.load = setting_window_load,
			.unload = setting_window_unload
		});
	}
}

static uint16_t setting_sub_menu_get_num_rows_callback(struct MenuLayer *menulayer, uint16_t section_index, void *callback_context){
	uint16_t ret = 0;
	switch(select_setting_option){
		case 0:  // distance
			ret = sizeof(search_distance_display_text)/sizeof(search_distance_display_text[0]);
			break;
		case 1:  // type
			ret = sizeof(search_type_display_text)/sizeof(search_type_display_text[0]);
			break;
		case 2:  // opennow
			ret = sizeof(search_opennow_display_text)/sizeof(search_opennow_display_text[0]);
			break;
		default:
			break;
	}
	return ret;
}

static void setting_sub_menu_draw_row_handler(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *callback_context){
	char *text = NULL;
	char *sub_text = "Marked";
	uint8_t radius, type, opennow;
	uint8_t selected_index = 0;  // the one that is selected previously by user
	
	get_search_options(s_search_data, &radius, &type, &opennow);

	switch(select_setting_option){
		case 0:  // distance
			selected_index = radius;
			text = search_distance_display_text[cell_index->row];
			break;
		case 1:  // type
			selected_index = type;
			text = search_type_display_text[cell_index->row];
			break;
		case 2:  // opennow
			selected_index = opennow;
			text = search_opennow_display_text[cell_index->row];
			break;
		default:
			break;
	}

	if(cell_index->row == selected_index)  // TODO: Add marker icon
		menu_cell_basic_draw(ctx, cell_layer, text, sub_text, NULL);
	else
		menu_cell_basic_draw(ctx, cell_layer, text, NULL, NULL);
}

static void setting_sub_menu_select_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context){
// Once an item is selected, update search_data and close this sub_menu window
	uint8_t radius, type, opennow;
	uint32_t old_search_data;

	old_search_data = s_search_data;
	get_search_options(s_search_data, &radius, &type, &opennow);
	switch(select_setting_option){
		case 0:  // distance
			radius = cell_index->row;
			break;
		case 1:  // type
			type = cell_index->row;
			break;
		case 2:  // opennow
			opennow = cell_index->row;
			break;
		default:
			break;
	}
	s_search_data = compute_search_data(radius, type, opennow);
	if(old_search_data != s_search_data){
		APP_LOG(APP_LOG_LEVEL_DEBUG, "Settings changed");
		is_setting_changed = true;
	}
	else
		is_setting_changed = false;

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
}

static void wait_animation_next_timer(void);

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
	graphics_context_set_stroke_color(ctx, GColorBlack);
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
	
	text_layer_set_font(s_wait_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
	text_layer_set_text_alignment(s_wait_text_layer, GTextAlignmentCenter);
	text_layer_set_background_color(s_wait_text_layer, GColorClear);
	text_layer_set_text_color(s_wait_text_layer, GColorBlack);
	text_layer_set_text(s_wait_text_layer, "Waiting");
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
}

static void wait_window_disappear(Window *window) {
	if(s_wait_animation_timer){
		app_timer_cancel(s_wait_animation_timer);
		s_wait_animation_timer = NULL;
	}
}

static void wait_window_push(void){
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
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context){
	// Read first item
	Tuple *t = dict_read_first(iterator);
	Window *top_window;
	int index;
	bool successFlag = false;
	char *l, *r, buf[256];

	APP_LOG(APP_LOG_LEVEL_INFO, "Message Received!");
	is_querying = false;  // reset the flag

	// store the list
	index = 0;
	while(t != NULL) {
		APP_LOG(APP_LOG_LEVEL_DEBUG, "key: %d Data: %s", (int)t->key, t->value->cstring);
		switch(t->key){
			case 0:
				strncpy(query_result, t->value->cstring, sizeof(query_result));
				break;
			default:
				if((t->key >= 1) && (t->key < (MAX_DATA_NUMBER+1))){
					// value string format: Restaurant name|Direction Distance
					strncpy(buf, t->value->cstring, sizeof(buf));
					l = r = buf;
					while(*r!='|')
						r++;
					*r = '\0';
					r++;

					strncpy(list_menu_text[index], l, sizeof(list_menu_text[index]));
					strncpy(list_menu_sub_text[index], r, sizeof(list_menu_sub_text[index]));
					strncat(list_menu_sub_text[index], " meters", 7);
					APP_LOG(APP_LOG_LEVEL_DEBUG, "index:%d menu_text:%s menu_sub_text:%s", index, list_menu_text[index], list_menu_sub_text[index]);
					index++;
				}
				else
					APP_LOG(APP_LOG_LEVEL_ERROR, "Invalid key: %d", (int)t->key);	
				break;
		}
		t = dict_read_next(iterator);
	}
	num_of_list_items = index;
	
	// Update last query time if we have success query
	if(!strncmp(query_result, "Success", 7)){
		last_query_time = time(NULL);
		is_setting_changed = false; // we have acquired valid data since last config changed
		successFlag = true;
	}

	// Check current window
	// If we are waiting, display the list or result window
	top_window = window_stack_get_top_window();
	if(top_window == s_wait_window){
		if(select_option == SELECT_OPTION_RANDOM){
			generate_random_result_window();
		}
		else if(select_option == SELECT_OPTION_LIST){
			// if we have failed query, still display result window with error message
			if(!successFlag)
				generate_random_result_window();
			else
				window_stack_push(s_list_window, true);
		}
		window_stack_remove(s_wait_window, false);
	}
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
	APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
	is_querying = false;
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
	APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
	is_querying = true;
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
	APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
	is_querying = false;  // reset the flag
}

static void init(){
	// Initialize random seed
	srand(time(NULL));

	// Initialize variables
	num_of_list_items = 0;
	last_query_time = 0;
	is_querying = false;
	is_setting_changed = false;
	select_option = 0;
	select_setting_option = 0;
	if(persist_exists(PERSIST_KEY_SEARCH_DATA)){
		s_search_data = persist_read_int(PERSIST_KEY_SEARCH_DATA);
		APP_LOG(APP_LOG_LEVEL_DEBUG, "search_data=%u", (unsigned int)s_search_data);
	}
	else{
		s_search_data = compute_search_data(DEFAULT_SEARCH_RADIUS, DEFAULT_SEARCH_TYPE, DEFAULT_SEARCH_OPENNOW);
		APP_LOG(APP_LOG_LEVEL_DEBUG, "default search_data=%u", (unsigned int)s_search_data);
	}

	// Create main window
	s_main_window = window_create();
	window_set_window_handlers(s_main_window, (WindowHandlers){
		.load = main_window_load,
		.unload = main_window_unload
	});






	// Register AppMessage callbacks
	app_message_register_inbox_received(inbox_received_callback);
	app_message_register_inbox_dropped(inbox_dropped_callback);
	app_message_register_outbox_failed(outbox_failed_callback);
	app_message_register_outbox_sent(outbox_sent_callback);
	app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());

	// Display main menu
	window_stack_push(s_main_window, true);
}

static void deinit(){
	// Destoy main window
	window_destroy(s_main_window);
	window_destroy(s_result_window);
	window_destroy(s_list_window);
	window_destroy(s_setting_window);
	window_destroy(s_wait_window);

	// Update Persist data
	persist_write_data(PERSIST_KEY_SEARCH_DATA, &s_search_data, sizeof(s_search_data));
}

int main(void) {
	init();
	app_event_loop();
	deinit();
}
