#include "main.h"
#include "localize.h"
#include "query.h"
#include "wait.h"
#include "result.h"
#include "detail.h"
#include "list.h"
#include "setting.h"

// --------------------------------------------------------------------------------------
// The constant strings
// --------------------------------------------------------------------------------------

const char *main_menu_text[MAIN_MENU_ROWS];
const char *main_banner_text;
const char *setting_main_menu_header_text;
const char *wait_banner_text;

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

GBitmap *icon_blank_bitmap;	// The blank icon for settings
GBitmap *icon_check_black_bitmap;	// The black check icon for settings
GBitmap *icon_agenda_bitmap;	// The icon for action bar

GColor text_color;
GColor bg_color;
GColor highlight_text_color;
GColor highlight_bg_color;
GColor highlight_alt_text_color;
GColor highlight_alt_bg_color;
	
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
void free_search_result(void){
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

// Reset the sorted index
void reset_sorted_index(void){
	int i;
	for(i=0;i<SEARCH_RESULT_MAX_DATA_NUMBER;i++){
		search_result.sorted_index[i] = i;
	}
}

// --------------------------------------------------------------------------------------
// Private data
// --------------------------------------------------------------------------------------

// The main menu with "Random" and "List" options
static Window *s_main_window;
static Layer *s_main_banner_layer;
static MenuLayer *s_main_menu_layer;

// --------------------------------------------------------------------------------------
// Private functions
// --------------------------------------------------------------------------------------

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
#ifdef PBL_ROUND
	graphics_draw_text(ctx, main_banner_text, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD), 
		GRect(bounds.origin.x, bounds.origin.y+MAIN_BANNER_TOP_MARGIN, bounds.size.w, bounds.size.h), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
#else
	graphics_draw_text(ctx, main_banner_text, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD), bounds,
			   GTextOverflowModeFill, GTextAlignmentCenter, NULL);
#endif
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
#ifdef PBL_ROUND
	// we only have 3 options, disable center focus looks better
	menu_layer_set_center_focused(s_main_menu_layer, false);
#endif

#ifdef PBL_COLOR
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
	if(top_window == get_wait_window()){
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
			window_stack_remove(get_wait_window(), false);
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
	wait_banner_text = _("Searching...");

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
#ifdef PBL_COLOR
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
