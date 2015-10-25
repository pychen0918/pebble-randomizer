#include "main.h"
#include "localize.h"
#include "query.h"

#define LIST_MENU_HEADER_HEIGHT 	22

#define SETTING_MENU_OPTION_RANGE	0
#define SETTING_MENU_OPTION_TYPE	1
#define SETTING_MENU_OPTION_OPENNOW	2
#define SETTING_MENU_HEADER_HEIGHT	22

#define WAIT_ANIMATION_TIMER_DELTA	33
#define WAIT_ANIMATION_BAR_LEFT_MARGIN	10
#define WAIT_ANIMATION_BAR_RIGHT_MARGIN	10
#define WAIT_ANIMATION_BAR_HEIGHT	6
#define WAIT_ANIMATION_BAR_RADIUS	(WAIT_ANIMATION_BAR_HEIGHT/2)
#define WAIT_WINDOW_TIMEOUT		25000	// Wait window should timeout in 25 seconds. js location looking has only 20 seconds timeout.
#define WAIT_TEXT_LAYER_HEIGHT		32

#define MESSAGE_UID_INVALID		0
#define MESSAGE_UID_FIRST		1
#define MESSAGE_UID_MAX			255

#define DEFAULT_SEARCH_RANGE		1	// 1km
#define DEFAULT_SEARCH_TYPE		1	// Restaurant
#define DEFAULT_SEARCH_OPENNOW		0	// do not add opennow filter

// --------------------------------------------------------------------------------------
// The constant strings
// --------------------------------------------------------------------------------------

const char *main_menu_text[MAIN_MENU_ROWS];
const char *main_banner_text;
const char *setting_main_menu_header_text;
const char *wait_layer_header_text;

const char *query_status_error_message[QUERY_STATUS_NUM_OF_ERROR_TYPES];
const char *query_status_error_sub_message[QUERY_STATUS_NUM_OF_ERROR_TYPES];

const char *unknown_error_message;
const char *unknown_error_sub_message;

const char *setting_main_menu_text[3];		// random, list, setting
const char *setting_range_option_text[4];	// 500 M, 1 KM, 5 KM, 10 KM
const char *setting_type_option_text[4];	// Food, Restaurant, Cafe, Bar
const char *setting_opennow_option_text[2];	// No, Yes

const char *detail_address_text;
const char *detail_phone_text;
const char *detail_rating_text;
const char *detail_nodata_text;
const char *detail_star_text;

const char *direction_name[9];			// N, NE, E, ..., NW, N, total 9
const char *distance_unit;

// --------------------------------------------------------------------------------------
// The global variables
// --------------------------------------------------------------------------------------

SearchResult search_result;	// The main data structure to store the returned search results
UserSetting user_setting;	// The main data structure to store custom searching configuations.
MenuState menu_state;		// The main data structure to store user's selection on the menu

static AppTimer *s_wait_animation_timer;// Timer for waiting animation
static int s_wait_animation_counter = 0;
static AppTimer *s_wait_timeout_timer;	// Timer for dismissing waiting window

static GBitmap *icon_blank_bitmap;	// The blank icon for settings
static GBitmap *icon_check_black_bitmap;	// The black check icon for settings
static GBitmap *icon_agenda_bitmap;	// The icon for action bar

static GColor text_color;
static GColor bg_color;
static GColor highlight_text_color;
static GColor highlight_bg_color;
static GColor highlight_alt_text_color;
static GColor highlight_alt_bg_color;
	
// The main menu with "Random" and "List" options
static Window *s_main_window;
static Layer *s_main_banner_layer;
static MenuLayer *s_main_menu_layer;

// The random pick result
static Window *s_result_window;
static TextLayer *s_result_title_text_layer;
static TextLayer *s_result_sub_text_layer;
static ActionBarLayer *s_result_action_bar_layer;
static ScrollLayer *s_result_scroll_layer;

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

// The detail window
static Window *s_detail_window;
static ScrollLayer *s_detail_scroll_layer;
static TextLayer *s_detail_text_layer;

// --------------------------------------------------------------------------------------
// Function Prototypes
// --------------------------------------------------------------------------------------
static void list_window_push(void);
static void result_window_push(void);
static void setting_window_push(void);
static void setting_sub_window_push(void);
static void wait_window_push(void);
static void detail_window_push(void);
static void wait_animation_next_timer(void);

// --------------------------------------------------------------------------------------
// Shared functions
// --------------------------------------------------------------------------------------

bool validate_data(void){
	time_t now = time(NULL);
	if(search_result.last_query_time > 0 && 					// must have query before
	   ((now - search_result.last_query_time) < SEARCH_RESULT_AGE_TIMEOUT) &&	// didn't timeout 
	   !(search_result.is_setting_changed))					// user haven't change settings
	{
		return true;
	}
	return false;
}

uint8_t get_next_uid(void){
	uint8_t temp;
	temp = search_result.uid_next;
	search_result.uid_next = (search_result.uid_next+1)%MESSAGE_UID_MAX;
	if(search_result.uid_next==MESSAGE_UID_INVALID)
		search_result.uid_next = MESSAGE_UID_FIRST;
	return temp;
}

uint8_t get_previous_uid(void){
	uint8_t temp;
	temp = (search_result.uid_next - 1 + MESSAGE_UID_MAX)%MESSAGE_UID_MAX;
	if(temp == MESSAGE_UID_INVALID)
		temp = MESSAGE_UID_MAX;
	return temp;
}


// allocate a space and copy string to that space. Return pointer to the space.
void *alloc_and_copy_string(char* string){
	char *ptr;
	int size = strlen(string)+1;
	ptr = (char *)malloc(size);
	strncpy(ptr, string, size);
	return ptr;
}

// search for certain store with place_id
int find_index_from_place_id(char *place_id){
	int index = -1;
	int i, num_of_rest;
	RestaurantInformation *ptr;
	
	ptr = search_result.restaurant_info;
	num_of_rest = search_result.num_of_restaurant;
	for( i = 0; i < num_of_rest; i++){
		if(((ptr+i)->place_id != NULL) && !strcmp(place_id, (ptr+i)->place_id)){
			index = i;
			break;
		}
	}
	return index;
}

// Free the dynamically allocated space in search_result
static void free_search_result(void){
	int i;
	RestaurantInformation *ptr = search_result.restaurant_info;

	for(i=0;i<SEARCH_RESULT_MAX_DATA_NUMBER;i++){
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
	if(search_result.api_error_message != NULL){
		free(search_result.api_error_message);
		search_result.api_error_message = NULL;
	}
}

static void reset_sorted_index(void){
	int i;
	for(i=0;i<SEARCH_RESULT_MAX_DATA_NUMBER;i++){
		search_result.sorted_index[i] = i;
	}
}

static void list_menu_sort_by_distance(void){
// sort the restaurants by distance and store the result in sorted_index array
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

static uint16_t main_menu_get_num_rows_callback(struct MenuLayer *menulayer, uint16_t section_index, void *callback_context){
	return MAIN_MENU_ROWS;
}

static void main_menu_draw_row_handler(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *callback_context){
	const char* text = main_menu_text[cell_index->row];

	menu_cell_basic_draw(ctx, cell_layer, text, NULL, NULL);
}

static void main_menu_select_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context){
	bool valid_result = false;
	bool is_querying = search_result.is_querying;

	valid_result = validate_data();
	menu_state.user_operation = cell_index->row;

	APP_LOG(APP_LOG_LEVEL_DEBUG, "Main Menu Select: %d", (int)(menu_state.user_operation));

	switch(menu_state.user_operation){
		case USER_OPERATION_RANDOM:
			if(valid_result)
				result_window_push();
			else{
				if(is_querying == false)
					send_list_query();
				free_search_result();
				wait_window_push();
			}
			break;
		case USER_OPERATION_LIST:
			if(valid_result)
				list_window_push();
			else{
				if(is_querying == false)
					send_list_query();
				free_search_result();
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

static void main_banner_layer_update_proc(Layer *layer, GContext *ctx){
	GRect bounds = layer_get_bounds(layer);

	graphics_context_set_text_color(ctx, highlight_alt_text_color);
	graphics_context_set_fill_color(ctx, highlight_alt_bg_color);
	graphics_fill_rect(ctx, bounds, MAIN_BANNER_HEIGHT, GCornerNone);
	graphics_draw_text(ctx, main_banner_text, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD), bounds,
			   GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

static void main_window_load(Window *window) {
	// Create Window's child Layers here
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(window_layer);

	APP_LOG(APP_LOG_LEVEL_INFO, "Menu load");

	s_main_banner_layer = layer_create(GRect(bounds.origin.x, bounds.origin.y, bounds.size.w, MAIN_BANNER_HEIGHT));
	layer_set_update_proc(s_main_banner_layer, main_banner_layer_update_proc);
	layer_add_child(window_layer, s_main_banner_layer);

	s_main_menu_layer = menu_layer_create(GRect(bounds.origin.x, bounds.origin.y+MAIN_BANNER_HEIGHT, 
						    bounds.size.w, bounds.size.h-MAIN_BANNER_HEIGHT));
#ifdef PBL_PLATFORM_BASALT
	menu_layer_set_normal_colors(s_main_menu_layer, bg_color, text_color);
	menu_layer_set_highlight_colors(s_main_menu_layer, highlight_bg_color, highlight_text_color);
	menu_layer_pad_bottom_enable(s_main_menu_layer, false);
#endif
	menu_layer_set_callbacks(s_main_menu_layer, NULL, (MenuLayerCallbacks){
		.get_num_rows = main_menu_get_num_rows_callback,
		.draw_row = main_menu_draw_row_handler,
		.select_click = main_menu_select_callback
	});
	menu_layer_set_click_config_onto_window(s_main_menu_layer, window);
	layer_add_child(window_layer, menu_layer_get_layer(s_main_menu_layer));
}

static void main_window_unload(Window *window) {
	layer_destroy(s_main_banner_layer);
	menu_layer_destroy(s_main_menu_layer);
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

	snprintf(sub_text, sizeof(sub_text), "%s %d %s", direction_name[ptr->direction], (int)(ptr->distance), distance_unit);

#ifdef PBL_PLATFORM_BASALT
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
	graphics_draw_text(ctx, text, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
		GRect(bounds.origin.x+5, bounds.origin.y-2, bounds.size.w-5, bounds.size.h),
		GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
	graphics_draw_text(ctx, sub_text, fonts_get_system_font(FONT_KEY_GOTHIC_18),
		GRect(bounds.origin.x+5, bounds.origin.y + bounds.size.h - 22, bounds.size.w, bounds.size.h),
		GTextOverflowModeFill, GTextAlignmentLeft, NULL);
}

static void list_menu_draw_header_handler(GContext *ctx, const Layer *cell_layer, uint16_t section_index, void *callback_context){
	graphics_context_set_text_color(ctx, highlight_alt_text_color);
	graphics_context_set_fill_color(ctx, highlight_alt_bg_color);
	graphics_fill_rect(ctx, layer_get_bounds(cell_layer), 0, GCornerNone);
	graphics_draw_text(ctx, setting_type_option_text[user_setting.type], fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), 
		layer_get_bounds(cell_layer), GTextOverflowModeFill, GTextAlignmentLeft, NULL); 
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
#ifdef PBL_PLATFORM_BASALT
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

static void result_window_push(void){
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
	graphics_draw_text(ctx, setting_main_menu_header_text, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), layer_get_bounds(cell_layer), GTextOverflowModeFill, GTextAlignmentLeft, NULL); 
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
#ifdef PBL_PLATFORM_BASALT
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

	if(cell_index->row == selected_index)
		menu_cell_basic_draw(ctx, cell_layer, text, NULL, icon_check_black_bitmap);
	else
		menu_cell_basic_draw(ctx, cell_layer, text, NULL, icon_blank_bitmap);
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
#ifdef PBL_PLATFORM_BASALT
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
#ifdef PBL_PLATFORM_BASALT
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

static void detail_window_push(void){
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

static void inbox_received_callback(DictionaryIterator *iterator, void *context){
	Tuple *t = dict_read_first(iterator);
	int parse_result = DATA_INVALID;
	uint8_t message_uid = MESSAGE_UID_INVALID, message_query_type = QUERY_TYPE_INVALID;
	uint8_t expect_uid = get_previous_uid();
	Window *top_window;

	APP_LOG(APP_LOG_LEVEL_INFO, "Message Received!");
	search_result.is_querying = false;  // reset the flag
	search_result.is_setting_changed = false;

	while(t!=NULL){
		if(t->key == KEY_QUERY_TYPE){
			message_query_type = t->value->uint8;
		}
		else if(t->key == KEY_QUERY_UID){
			message_uid = t->value->uint8;
		}
		t = dict_read_next(iterator);
	}

	APP_LOG(APP_LOG_LEVEL_DEBUG, "query_type:%d uid:%d", (int)message_query_type, (int)message_uid);

	if(message_query_type == QUERY_TYPE_LIST)
		parse_result = parse_list_message_handler(iterator);
	else if(message_query_type == QUERY_TYPE_DETAIL)
		parse_result = parse_detail_message_handler(iterator);
	else
		APP_LOG(APP_LOG_LEVEL_ERROR, "Unknown query type");

	APP_LOG(APP_LOG_LEVEL_DEBUG, "parse_result=%d query_status=%d", parse_result, search_result.query_status);

	// Check current window
	// If we are waiting, display the list, result or detail window
	top_window = window_stack_get_top_window();
	if(top_window == s_wait_window){
		APP_LOG(APP_LOG_LEVEL_DEBUG, "Is waiting");
		// Must make sure we received the query that we expected before display
		if(expect_uid == message_uid){
			APP_LOG(APP_LOG_LEVEL_DEBUG, "UID matched");
			if(menu_state.user_operation == USER_OPERATION_RANDOM){
				APP_LOG(APP_LOG_LEVEL_DEBUG, "operation random");
				result_window_push();
			}
			else if(menu_state.user_operation == USER_OPERATION_LIST){
				// if the result is invalid, still display result window with error message
				if(parse_result != DATA_VALID)
					result_window_push();
				else
					list_window_push();
			}
			else if(menu_state.user_operation == USER_OPERATION_DETAIL){
				// if the result is invalid, still display result window with error message
				if(parse_result != DATA_VALID)
					result_window_push();
				else
					detail_window_push();
			}
			window_stack_remove(s_wait_window, false);
		}
	}
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
	APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
	search_result.is_querying = false;
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
	APP_LOG(APP_LOG_LEVEL_DEBUG, "Outbox send success!");
	search_result.is_querying = true;
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
	APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
	search_result.is_querying = false;  // reset the flag
}

static void initialize_const_strings(void){
	main_menu_text[0] = _("Pick For Me!");
	main_menu_text[1] = _("List All");
	main_menu_text[2] = _("Options");
	main_banner_text = _("Where to eat?");
	setting_main_menu_header_text = _("Options");
	wait_layer_header_text = _("Searching...");

	query_status_error_message[0] = "";
	query_status_error_message[1] = _("No Result");
	query_status_error_message[2] = _("GPS Timeout");
	query_status_error_message[3] = _("API Error");
	query_status_error_message[4] = _("No Result");
	query_status_error_sub_message[0] = "";
	query_status_error_sub_message[1] = _("Please try other search options");
	query_status_error_sub_message[2] = _("Please try again later");
	query_status_error_sub_message[3] = "";
	query_status_error_sub_message[4] = _("Cannot find any information about this restraunt");

	unknown_error_message = _("Unknown error");
	unknown_error_sub_message = _("Please bring the following message to the author:");

	setting_main_menu_text[0] = _("Range");
	setting_main_menu_text[1] = _("Keyword");
	setting_main_menu_text[2] = _("Open Now Only");
	setting_range_option_text[0] = _("500 M");
	setting_range_option_text[1] = _("1 KM");
	setting_range_option_text[2] = _("5 KM");
	setting_range_option_text[3] = _("10 KM");
	setting_type_option_text[0] = _("Food");
	setting_type_option_text[1] = _("Restaurant");
	setting_type_option_text[2] = _("Cafe");
	setting_type_option_text[3] = _("Bar");
	setting_opennow_option_text[0] = _("No");
	setting_opennow_option_text[1] = _("Yes");

	detail_address_text = _("Address: ");
	detail_phone_text = _("Tel: ");
	detail_rating_text = _("Rating: ");
	detail_nodata_text = _("No data");
	detail_star_text = _("stars");

	direction_name[0] = _("N");
	direction_name[1] = _("NE");
	direction_name[2] = _("E");
	direction_name[3] = _("SE");
	direction_name[4] = _("S");
	direction_name[5] = _("SW");
	direction_name[6] = _("W");
	direction_name[7] = _("NW");
	direction_name[8] = _("N");
	distance_unit = _("meters");
}

static void initialize_color(void){
#ifdef PBL_PLATFORM_BASALT
	text_color = GColorBlack;
	bg_color = GColorClear;
	highlight_text_color = GColorBlack;
	highlight_bg_color = GColorRajah;
	highlight_alt_text_color = GColorWhite;
	highlight_alt_bg_color = GColorDukeBlue;
#else
	text_color = GColorBlack;
	bg_color = GColorClear;
	highlight_text_color = GColorBlack;
	highlight_bg_color = GColorClear;
	highlight_alt_text_color = GColorBlack;
	highlight_alt_bg_color = GColorClear;
#endif
}

static void init(){
	// Initialize locale framework
	locale_init();

	initialize_const_strings();

	initialize_color();

	// Initialize random seed
	srand(time(NULL));

	// Initialize variables
	memset(search_result.restaurant_info, 0, sizeof(search_result.restaurant_info));
	reset_sorted_index();
	search_result.is_querying = false;
	search_result.last_query_time = 0;
	search_result.num_of_restaurant = 0;
	search_result.query_status = QUERY_STATUS_NO_RESULT;
	search_result.random_result = 0;
	search_result.uid_next = MESSAGE_UID_FIRST;
	search_result.is_querying = false;
	search_result.is_setting_changed = false;
	if(persist_exists(PERSIST_KEY_USER_SETTING))
		persist_read_data(PERSIST_KEY_USER_SETTING, &user_setting, sizeof(user_setting));
	else{
		user_setting.range = DEFAULT_SEARCH_RANGE;
		user_setting.type = DEFAULT_SEARCH_TYPE;
		user_setting.opennow = DEFAULT_SEARCH_OPENNOW;
	}
	menu_state.user_operation = USER_OPERATION_RANDOM;
	menu_state.setting_menu_selected_option = SETTING_MENU_OPTION_RANGE;
	menu_state.user_detail_index = 0;

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
	persist_write_data(PERSIST_KEY_USER_SETTING, &user_setting, sizeof(user_setting));
}

int main(void) {
	init();
	app_event_loop();
	deinit();
}
