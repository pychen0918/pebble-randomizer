#include <pebble.h>

#define SEARCH_RESULT_MAX_DATA_NUMBER	20	// Max number of returned data
#define SEARCH_RESULT_AGE_TIMEOUT	60 	// How long the list is valid in seconds

#define MAIN_MENU_ROWS			3	// Random, list and settings
#define MAIN_MENU_OPTION_RANDOM		0
#define MAIN_MENU_OPTION_LIST		1
#define MAIN_MENU_OPTION_SETTING	2
#define MAIN_MENU_TEXT_LENGTH		64

// What kind of operation user is currently performing
#define USER_OPERATION_RANDOM		0
#define USER_OPERATION_LIST		1
#define USER_OPERATION_SETTING		2
#define USER_OPERATION_DETAIL		3

#define LIST_MENU_ROWS  		SEARCH_RESULT_MAX_DATA_NUMBER
#define LIST_MENU_TEXT_LENGTH		128
#define LIST_MENU_SUB_TEXT_LENGTH	16
#define LIST_MENU_HEADER_HEIGHT 	18

#define SETTING_MENU_ROWS		3	// Range, Type, Open Now
#define SETTING_MENU_OPTION_RANGE	0
#define SETTING_MENU_OPTION_TYPE	1
#define SETTING_MENU_OPTION_OPENNOW	2
#define SETTING_MENU_TEXT_LENGTH	64
#define SETTING_MENU_SUB_TEXT_LENGTH	64
#define SETTING_MENU_HEADER_HEIGHT	18

#define WAIT_ANIMATION_TIMER_DELTA	33
#define WAIT_ANIMATION_BAR_LEFT_MARGIN	10
#define WAIT_ANIMATION_BAR_RIGHT_MARGIN	10
#define WAIT_ANIMATION_BAR_HEIGHT	6
#define WAIT_ANIMATION_BAR_RADIUS	(WAIT_ANIMATION_BAR_HEIGHT/2)
#define WAIT_TEXT_LAYER_HEIGHT		32

#define PERSIST_KEY_USER_SETTING	40

// XXX: If changed the following status code, the js file might need to be updated
#define QUERY_STATUS_SUCCESS		0
#define QUERY_STATUS_NO_RESULT		1
#define QUERY_STATUS_GPS_TIMEOUT	2
#define QUERY_STATUS_GOOGLE_API_ERROR	3
#define QUERY_TYPE_LIST			0	// ask for the information 20 nearby store
#define QUERY_TYPE_DETAIL		1	// ask for the information of one certain store

#define DATA_VALID			0
#define DATA_INVALID			1

#define DEFAULT_SEARCH_RANGE		1	// 1km
#define DEFAULT_SEARCH_TYPE		1	// Restaurant
#define DEFAULT_SEARCH_OPENNOW		0	// do not add opennow filter

// Key for appmessage
#define KEY_STATUS			0
#define KEY_QUERY_TYPE			1	
#define KEY_QUERY_PLACE_ID		2	// store place id
#define KEY_QUERY_ERROR_MESSAGE		3
#define KEY_QUERY_UID			4	// unique id for each sent/received messages pair
#define KEY_QUERY_OPTION_RANGE		10
#define KEY_QUERY_OPTION_TYPE		11
#define KEY_QUERY_OPTION_OPENNOW	12
#define KEY_DETAIL_ADDRESS		20	// detail information returned by 'detail' query
#define KEY_DETAIL_PHONE		21
#define KEY_DETAIL_RATING		22
#define KEY_LIST_FIRST			30	// First restaurant information data (returned by 'list' query)

// --------------------------------------------------------------------------------------
// XXX: Data Structure Defination
// --------------------------------------------------------------------------------------

typedef struct __restaurant_information_t{
	char *name;				// name of the restaurant
	char *place_id;				// the Google place_id. Need for searching telphone number
	char *address;				// the short address
	char *phone;				// the telphone number
	uint16_t distance;			// distance between current location and the restraunt. Roundup to 10 meters.
	uint8_t rating;				// user rating of the store. Interger between 0 ~ 5
	uint8_t direction;			// compass direction from current loation, e.g., N, NE, E, ...
} RestaurantInformation;

typedef struct __search_result_t{
	RestaurantInformation restaurant_info[SEARCH_RESULT_MAX_DATA_NUMBER];	// The above restaurant information. Up to 20 restaurants.
	time_t last_query_time;			// The epoch time of the last successful query. Use to check if the data is valid or not.
	uint8_t num_of_restaurant;		// How many restaurants actually found
	uint8_t query_status;			// Record most recent query status. It is for list query only, not detail.
	uint8_t random_result;			// Record the index of random picked restaurant. Between 0 ~ (num_of_restaurant-1)
	uint8_t uid_next;			// The uid id for next message. Each send message must have unique id
	char *api_error_message;		// Store Google API's return error message. Might be empty.
	bool is_querying;			// If we are currently waiting for query result return, do not send another one.
	bool is_setting_changed;		// if user has made changes after last query
} SearchResult;

typedef struct __user_setting_t{
	uint8_t range;				// Index of selected range (0: 500m, 1: 1km, 2: 5km, 3: 10km)
	uint8_t type;				// Index of selected type (0: Food, 1: Restaurant, 2: Cafe, 3: Bar)
	uint8_t opennow;			// Index of selected opennow filter (0: Disable, 1: Enable)
} UserSetting;

typedef struct __menu_status_t{
	uint8_t user_operation;		 	// which function is currently performed by user (Random, List, Setting, Detail)
	uint8_t setting_menu_selected_option; 	// which setting menu is selected by user (Range, Keyword, Open Now)
} MenuState;

// --------------------------------------------------------------------------------------
// XXX: The constant strings
// --------------------------------------------------------------------------------------

const char main_menu_text[MAIN_MENU_ROWS][MAIN_MENU_TEXT_LENGTH] = {"Random!", "List", "Settings"};
const char *list_menu_header_text = "Restaurants";
const char *setting_main_menu_header_text = "Options";
const char *wait_layer_header_text = "Searching...";

// XXX: must match QUERY_STATUS_xxx index
const char query_status_error_message[][256] = {"", "No Result", "GPS Timeout", "API Error"};
const char query_status_error_sub_message[][256] = {"", "Please try other search options", "Please try again later", ""};

const char unknown_error_message[256] = "Unknown error";
const char unknown_error_sub_message[256] = "Please bring the following message to the author:";

const char setting_main_menu_text[][SETTING_MENU_TEXT_LENGTH] = {"Range", "Keyword", "Open Now"};
const char setting_range_option_text[][LIST_MENU_SUB_TEXT_LENGTH] = {"500 M", "1 KM", "5 KM", "10 KM"};
const char setting_type_option_text[][LIST_MENU_SUB_TEXT_LENGTH] = {"Food", "Restaurant", "Cafe", "Bar"};
const char setting_opennow_option_text[][LIST_MENU_SUB_TEXT_LENGTH] = {"No", "Yes"};

const char direction_name[][16] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW", "N"};
const char distance_unit[16] = "meters";

// --------------------------------------------------------------------------------------
// XXX: The global variables
// --------------------------------------------------------------------------------------

static SearchResult s_search_result;	// The main data structure to store the returned search results
static UserSetting s_user_setting;	// The main data structure to store custom searching configuations.
static MenuState s_menu_state;		// The main data structure to store user's selection on the menu

static AppTimer *s_wait_animation_timer;// Timers for waiting animation
static int s_wait_animation_counter = 0;

// The main menu with "Random" and "List" options
static Window *s_main_window;
static MenuLayer *s_main_menu_layer;

// The random pick result
static Window *s_result_window;
static TextLayer *s_result_title_text_layer;
static TextLayer *s_result_sub_text_layer;

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

// --------------------------------------------------------------------------------------
// XXX: Common functions
// --------------------------------------------------------------------------------------

// Check if current stored restaurant data is valid.
static bool validate_data(void){
	time_t now = time(NULL);
	if(s_search_result.last_query_time > 0 && 					// must have query before
	   ((now - s_search_result.last_query_time) < SEARCH_RESULT_AGE_TIMEOUT) &&	// didn't timeout 
	   !(s_search_result.is_setting_changed))					// user haven't change settings
	{
		return true;
	}
	return false;
}

// Send out the 'list' query to retrieve 20 nearby restaurant info
static void send_list_query(void){
	DictionaryIterator *iter;
	app_message_outbox_begin(&iter);
	dict_write_uint8(iter, KEY_QUERY_TYPE, QUERY_TYPE_LIST);
	dict_write_uint8(iter, KEY_QUERY_UID, s_search_result.uid_next++);
	dict_write_uint8(iter, KEY_QUERY_OPTION_RANGE, s_user_setting.range);
	dict_write_uint8(iter, KEY_QUERY_OPTION_TYPE, s_user_setting.type);
	dict_write_uint8(iter, KEY_QUERY_OPTION_OPENNOW, s_user_setting.opennow);
	app_message_outbox_send();
}

// Send out the 'detail' query to retrieve certain restaurant detailed info
// Input is the index of the store in the list
static void send_detail_query(uint8_t store_index){
	DictionaryIterator *iter;
	app_message_outbox_begin(&iter);
	dict_write_uint8(iter, KEY_QUERY_TYPE, QUERY_TYPE_DETAIL);
	dict_write_uint8(iter, KEY_QUERY_UID, s_search_result.uid_next++);
	dict_write_cstring(iter, KEY_QUERY_PLACE_ID, s_search_result.restaurant_info[store_index].place_id);
	app_message_outbox_send();
}

// allocate a space and copy string to that space. Return pointer to the space.
static void *alloc_and_copy_string(char* string){
	char *ptr;
	int size = strlen(string)+1;
	ptr = (char *)malloc(size);
	strncpy(ptr, string, size-1);
	return ptr;
}

// search for certain store with place_id
static int find_index_from_place_id(char *place_id){
	int index = -1;
	int i, num_of_rest;
	RestaurantInformation *ptr;
	
	ptr = s_search_result.restaurant_info;
	num_of_rest = s_search_result.num_of_restaurant;
	for( i = 0; i < num_of_rest; i++){
		if(((ptr+i)->place_id != NULL) && !strcmp(place_id, (ptr+i)->place_id)){
			index = i;
			break;
		}
	}
	return index;
}

// Free the dynamically allocated space in s_search_result
static void free_search_result(void){
	int i;
	RestaurantInformation *ptr = s_search_result.restaurant_info;

	for(i=0;i<MAX_DATA_NUMBER;i++){
		if((ptr+i)->name != NULL){
			free((ptr+i)->name);
			(ptr+i)->name = NULL;
		}
		if((ptr+i)->place_id != NULL){
			free((ptr+i)->place_id);
			(ptr+i)->place_id = NULL;
		}
		if((ptr+i)->address != NULL){
			free((ptr+i)->address);
			(ptr+i)->address = NULL;
		}
		if((ptr+i)->phone != NULL){
			free((ptr+i)->phone);
			(ptr+i)->phone = NULL;
		}
	}
	if(s_search_result.api_error_message != NULL){
		free(s_search_result.api_error_message);
		s_search_result.api_error_message = NULL;
	}
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
	bool is_querying = s_search_result.is_querying;

	APP_LOG(APP_LOG_LEVEL_INFO, "Select Click");

	valid_result = validate_data();
	s_menu_state.main_menu_selected_option = cell_index->row;

	// free the space if the data are invalid
	if(valid_result == false)
		free_search_result();

	switch(cell_index->row){
		case USER_OPERATION_RANDOM:
			if(valid_result)
				result_window_push();
			else{
				if(is_querying == false)
					send_query();
				wait_window_push();
			}
			break;
		case USER_OPERATION_LIST:
			if(valid_result)
				list_window_push();
			else{
				if(is_querying == false)
					send_query();
				wait_window_push();
			}
			break;
		case USER_OPERATION_SETTING:
			setting_window_push();
			break;
		default:
			APP_LOG(APP_LOG_LEVEL_ERROR, "Main menu invalid selection: %d", (int)(cell_index->row));
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
	return s_search_result.num_of_restaurant;
}

static int16_t list_menu_get_header_height_callback(struct MenuLayer *menu_layer, uint16_t section_index, void *callback_context){
	return LIST_MENU_HEADER_HEIGHT;
}

static void list_menu_draw_row_handler(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *callback_context){
	RestaurantInformation *ptr = &(s_search_result.restaurant_info[cell_index->row]);
	char *text = ptr->name;
	char sub_text[256];

	sprintf(sub_text, "%s %d %s", direction_name[ptr->direction], (int)(ptr->distance), distance_unit);

	menu_cell_basic_draw(ctx, cell_layer, text, sub_text, NULL);
}

static void list_menu_draw_header_handler(GContext *ctx, const Layer *cell_layer, uint16_t section_index, void *callback_context){
	menu_cell_basic_header_draw(ctx, cell_layer, list_menu_header_text);
}

// TODO: add detail window
static void list_menu_select_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context){
	int index = cell_index->row;

	APP_LOG(APP_LOG_LEVEL_INFO, "%d item selected", cell_index->row);

	// Check if we have detail information already
	if(s_search_result.restaurant_info[index].address != NULL){
		// show detail window
		// detail_window_push();
	}
	else{
		// send_detail_query((uint8_t)index);
		// s_menu_state.user_operation = USER_OPERATION_DETAIL;
		// push_wait_window();
	}
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
	window_stack_push(s_list_window, true);
}

// TODO: Register button press for detail
static void result_window_load(Window *window) {
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(window_layer);
	char title_text[256], sub_text[256];
	RestaurantInformation *ptr;
	uint8_t status;
	
	APP_LOG(APP_LOG_LEVEL_INFO, "Result load");

	status = s_search_result.query_status;
	switch(status){
		case QUERY_STATUS_SUCCESS:
			// Randomly pick up the restaurant
			s_search_result.random_result = rand()%s_search_result.num_of_restaurant;
			// Collect required fields
			ptr = s_search_result.restaurant_info[s_search_result.random_result];
			strncpy(title_text, ptr->name, sizeof(title_text));
			snprintf(sub_text, sizeof(sub_text), "%s %u %s", direction_name[ptr->direction], (unsigned int)(ptr->distance), distance_unit);
			break;
		case QUERY_STATUS_NO_RESULT:
		case QUERY_STATUS_GPS_TIMEOUT:
			// Set corresponding error messages to title and subtitle
			strncpy(title_text, query_status_error_message[status], sizeof(title_text));
			strncpy(sub_text, query_status_sub_error_message[status], sizeof(sub_text));
			break;
		case QUERY_STATUS_GOOGLE_API_ERROR:
			// Collect error message from returned information
			strncpy(title_text, query_status_error_message[status], sizeof(title_text));
			strncpy(sub_text, ptr->api_error_message, sizeof(sub_text));
			break;
		default:
			// Other query status = unknown error
			strncpy(title_text, unknown_error_message, sizeof(title_text));
			snprintf(sub_text, sizeof(sub_text), "%s %s%d", unknown_error_sub_message, "Incorrect query status:",status);
			break;
	}

	// title part
	s_result_title_text_layer = text_layer_create(GRect(bounds.origin.x, bounds.origin.y, bounds.size.w, (bounds.size.h/2)));
	text_layer_set_font(s_result_title_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
	text_layer_set_text_alignment(s_result_title_text_layer, GTextAlignmentLeft);
	text_layer_set_background_color(s_result_title_text_layer, GColorClear);
	text_layer_set_text_color(s_result_title_text_layer, GColorBlack);
	text_layer_set_text(s_result_title_text_layer, title_text);
	layer_add_child(window_layer, text_layer_get_layer(s_result_title_text_layer));
	// sub title part
	s_result_sub_text_layer = text_layer_create(GRect(bounds.origin.x, bounds.origin.y+(bounds.size.h/2), bounds.size.w, (bounds.size.h/2)));
	text_layer_set_font(s_result_sub_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
	text_layer_set_text_alignment(s_result_sub_text_layer, GTextAlignmentLeft);
	text_layer_set_background_color(s_result_sub_text_layer, GColorClear);
	text_layer_set_text_color(s_result_sub_text_layer, GColorBlack);
	text_layer_set_text(s_result_sub_text_layer, sub_text);
	layer_add_child(window_layer, text_layer_get_layer(s_result_sub_text_layer));
}

static void result_window_unload(Window *window) {
	text_layer_destroy(s_result_title_text_layer);
	text_layer_destroy(s_result_sub_text_layer);

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
	window_stack_push(s_result_window, true);
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
	
	switch(cell_index->row){
		case SETTING_MENU_OPTION_RANGE:
			sub_text = search_distance_display_text[s_user_setting.range];
			break;
		case SETTING_MENU_OPTION_TYPE:
			sub_text = search_type_display_text[s_user_setting.type];
			break;
		case SETTING_MENU_OPTION_OPENNOW:
			sub_text = search_opennow_display_text[s_user_setting.opennow];
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
	s_menu_state.setting_menu_selected_option = cell_index->row;

	setting_sub_window_push();
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
	window_stack_push(s_setting_window, true);
}

static uint16_t setting_sub_menu_get_num_rows_callback(struct MenuLayer *menulayer, uint16_t section_index, void *callback_context){
	uint16_t ret = 0;
	switch(s_menu_state.setting_menu_selected_option){
		case SETTING_MENU_OPTION_RANGE:
			ret = sizeof(search_distance_display_text)/sizeof(search_distance_display_text[0]);
			break;
		case SETTING_MENU_OPTION_TYPE:
			ret = sizeof(search_type_display_text)/sizeof(search_type_display_text[0]);
			break;
		case SETTING_MENU_OPTION_OPENNOW:
			ret = sizeof(search_opennow_display_text)/sizeof(search_opennow_display_text[0]);
			break;
		default:
			break;
	}
	return ret;
}

// TODO: Add marker icon
static void setting_sub_menu_draw_row_handler(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *callback_context){
	char *text = NULL;
	char *sub_text = "Marked";
	uint8_t selected_index = 0;  // the one that is selected previously by user

	switch(s_menu_state.setting_menu_selected_option){
		case SETTING_MENU_OPTION_RANGE:
			selected_index = s_user_setting.range;
			text = search_distance_display_text[cell_index->row];
			break;
		case SETTING_MENU_OPTION_TYPE:
			selected_index = s_user_setting.type;
			text = search_type_display_text[cell_index->row];
			break;
		case SETTING_MENU_OPTION_OPENNOW:
			selected_index = s_user_setting.opennow;
			text = search_opennow_display_text[cell_index->row];
			break;
		default:
			break;
	}

	if(cell_index->row == selected_index)
		menu_cell_basic_draw(ctx, cell_layer, text, sub_text, NULL);
	else
		menu_cell_basic_draw(ctx, cell_layer, text, NULL, NULL);
}

static void setting_sub_menu_select_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context){
	UserSetting old;

	memcpy(old, s_user_setting, sizeof(old));

	switch(s_menu_state.setting_menu_selected_option){
		case SETTING_MENU_OPTION_RANGE:
			s_user_setting.range = cell_index->row;
			break;
		case SETTING_MENU_OPTION_TYPE:
			s_user_setting.range = cell_index->row;
			break;
		case SETTING_MENU_OPTION_OPENNOW:
			s_user_setting.range = cell_index->row;
			break;
		default:
			break;
	}
	if(memcmp(s_user_setting, old, sizeof(s_user_setting))){
		APP_LOG(APP_LOG_LEVEL_DEBUG, "Settings changed");
		s_search_result.is_setting_changed = true;
	}
	else
		s_search_result.is_setting_changed = false;

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
}

static void wait_window_disappear(Window *window) {
	if(s_wait_animation_timer){
		app_timer_cancel(s_wait_animation_timer);
		s_wait_animation_timer = NULL;
	}
}

// TODO: add a timeout so we won't keep waiting for message
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
	window_stack_push(s_wait_window, true);
}

// Parse the returned list dictionary data and store to s_search_result
static int parse_list_message_handler(DictionaryIterator *iterator){
	Tuple *t = dict_read_first(iterator);
	int index;
	int ret = DATA_INVALID;
	RestaurantInformation *ptr;
	char buf[2][256];
	char errmsg[256];

	APP_LOG(APP_LOG_LEVEL_DEBUG, "parse_list_message_handler");
	
	index = 0;
	while(t != NULL) {
		switch(t->key){
			case KEY_STATUS:
				s_search_result.query_status = t->value->uint8;
				break;
			case KEY_QUERY_ERROR_MESSAGE:
				strncpy(errmsg, t->value->cstring, sizeof(errmsg));
				break;
			default:
				//  Check if the key fall in the data range
				if((t->key >= KEY_LIST_FIRST) && (t->key < (KEY_LIST_FIRST + MAX_DATA_NUMBER))){
					ptr = s_search_result.restaurant_info[index];
					// value string format: Restaurant name|Direction|Distance|placeid
					sscanf(t->value->cstring, "%s|%u|%u|%s", buf[0], ptr->direction, ptr->distance, buf[1]);
					ptr->name = alloc_and_copy_string(buf[0]);
					ptr->place_id = alloc_and_copy_string(buf[1]);

					APP_LOG(APP_LOG_LEVEL_DEBUG, "index:%d key:%d name:%s direction:%d distance:%d place_id:%s", 
						index, t->key, ptr->name, ptr->direction, ptr->distance, ptr->place_id);
					index++;
				}
				else
					APP_LOG(APP_LOG_LEVEL_ERROR, "Invalid key: %d", (int)t->key);
				break;
		}
		t = dict_read_next(iterator);
	}
	s_search_result.num_of_restaurant = index;

	// Update some information if query success
	if(s_search_result.query_status == QUERY_STATUS_SUCCESS){
		s_search_result.last_query_time = time(NULL);
		s_search_result.is_setting_changed = false;
		ret = DATA_VALID;
	}
	else if(s_search_result.query_status == QUERY_STATUS_GOOGLE_API_ERROR){
		s_search_result.api_error_message = alloc_and_copy_string(errmsg);
	}

	return ret;
}

// Parse the returned list dictionary data and store to s_search_result
static int parse_detail_message_handler(DictionaryIterator *iterator){
	Tuple *t = dict_read_first(iterator);
	uint8_t rating;
	char buf[2][256];
	char errmsg[256];
	int index = -1;
	int ret = DATA_INVALID;

	APP_LOG(APP_LOG_LEVEL_DEBUG, "parse_list_message_handler");

	switch(t->key){
		case KEY_STATUS:
			s_search_result.query_status = t->value->uint8;
			break;
		case KEY_QUERY_ERROR_MESSAGE:
			strncpy(errmsg, t->value->cstring, sizeof(errmsg));
			break;
		case KEY_QUERY_PLACE_ID:
			// user might send multiple detail queries in once. Use the returned place_id as the key
			index = find_index_from_place_id(t->value->cstring);
			break;
		case KEY_DETAIL_ADDRESS:
			strncpy(buf[0], t->value->cstring, sizeof(buf[0]));
			break;
		case KEY_DETAIL_PHONE:
			strncpy(buf[1], t->value->cstring, sizeof(buf[1]));
			break;
		case KEY_DETAIL_RATING:
			rating = t->value->uint8;
			break;
		default:
			APP_LOG(APP_LOG_LEVEL_ERROR, "Invalid key: %d", (int)t->key);
			break;
	}

	// if we failed to find corresponding restaurant, consider this query returns no result
	if(index < 0){
		s_search_result.query_status = QUERY_STATUS_NO_RESULT;
	}

	// Store the information if everything is OK
	if(s_search_result.query_status == QUERY_STATUS_SUCCESS){
		s_search_result.restaurant_info[index].address = alloc_and_copy_string(buf[0]);
		s_search_result.restaurant_info[index].phone = alloc_and_copy_string(buf[1]);
		s_search_result.restaurant_info[index].rating = rating;
		ret = DATA_VALID;
	}
	else if(s_search_result.query_status == QUERY_STATUS_GOOGLE_API_ERROR){
		s_search_result.api_error_message = alloc_and_copy_string(errmsg);
	}
	return ret;
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context){
	Tuple *t = dict_read_first(iterator);
	int parse_result = DATA_INVALID;
	uint8_t message_uid, message_query_type;
	uint8_t expect_uid = (uint8_t)(s_search_result.uid_next - 1);
	Window *top_window;

	APP_LOG(APP_LOG_LEVEL_INFO, "Message Received!");
	s_search_result.is_querying = false;  // reset the flag
	s_search_result.is_setting_changed = false;

	while(t!=NULL){
		if(t->key == KEY_QUERY_TYPE)
			message_query_type = t->value->uint8;
		else if(t->key == KEY_QUERY_UID)
			message_uid = t->value->uint8;
		t = dict_read_next(iterator);
	}

	if(returned_query_type == QUERY_TYPE_LIST)
		parse_result = parse_list_message_handler(iterator);
	else if(returned_query_type == QUERY_TYPE_DETAIL)
		parse_result = parse_detail_message_handler(iterator);
	else
		APP_LOG(APP_LOG_LEVEL_ERROR, "Unknown query type");

	// Check current window
	// If we are waiting, display the list, result or detail window
	top_window = window_stack_get_top_window();
	if(top_window == s_wait_window){
		// Must make sure we received the query that we expected before display
		if(expect_uid == message_uid){
			if(s_menu_state->user_operation == USER_OPERATION_RANDOM){
				result_window_push();
			}
			else if(s_menu_state->user_operation == USER_OPERATION_LIST){
				// if the result is invalid, still display result window with error message
				if(parse_result != DATA_VALID)
					result_window_push();
				else
					list_window_push();
			}
			else if(s_menu_state->user_operation == USER_OPERATION_DETAIL){
				// if the result is invalid, still display result window with error message
				//if(parse_result != DATA_VALID)
				//	result_window_push();
				//else
				//	detail_window_push();
			}
			window_stack_remove(s_wait_window, false);
		}
	}
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
	APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
	s_search_result.is_querying = false;
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
	APP_LOG(APP_LOG_LEVEL_DEBUG, "Outbox send success!");
	s_search_result.is_querying = true;
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
	APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
	s_search_result.is_querying = false;  // reset the flag
}

static void init(){
	// Initialize random seed
	srand(time(NULL));

	// Initialize variables
	memset(s_search_result.restaurant_info, 0, sizeof(s_search_result.restaurant_info));
	s_search_result.is_querying = false;
	s_search_result.last_query_time = 0;
	s_search_result.num_of_restaurant = 0;
	s_search_result.query_status = QUERY_STATUS_NO_RESULT;
	s_search_result.random_result = 0;
	s_search_result.uid_next = 0;
	s_search_result.is_querying = false;
	s_search_result.is_setting_changed = false;
	if(persist_exists(PERSIST_KEY_USER_SETTING))
		persist_read_data(PERSIST_KEY_USER_SETTING, &s_user_setting);
	else{
		s_user_setting.range_index = DEFAULT_SEARCH_RANGE;
		s_user_setting.type_index = DEFAULT_SEARCH_TYPE;
		s_user_setting.opennow_index = DEFAULT_SEARCH_OPENNOW;
	}
	s_menu_state.main_menu_selected_option = MAIN_MENU_OPTION_RANDOM;
	s_menu_state.setting_menu_selected_option = SETTING_MENU_OPTION_RANGE;

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

	// Free allocated space
	free_search_result();

	// Update Persist data
	persist_write_data(PERSIST_KEY_USER_SETTING, &s_user_setting, sizeof(s_user_setting));
}

int main(void) {
	init();
	app_event_loop();
	deinit();
}
