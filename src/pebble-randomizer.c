#include <pebble.h>
#define MAX_DATA_NUMBER 20 		// Max number of returned data
#define MAIN_MENU_ROWS	2  		// Random and list
#define LIST_MENU_ROWS  MAX_DATA_NUMBER
#define RESULT_AGE_TIME	300 		// How long the list is valid in seconds
#define SELECT_OPTION_RANDOM	0
#define SELECT_OPTION_LIST	1
#define MAIN_MENU_TEXT_LENGTH	64
#define LIST_MENU_TEXT_LENGTH	128
#define LIST_MENU_SUB_TEXT_LENGTH	16
#define LIST_MENU_HEADER_HEIGHT 18

static char main_menu_text[MAIN_MENU_ROWS][MAIN_MENU_TEXT_LENGTH] = {"Random!","List"};
static char list_menu_text[LIST_MENU_ROWS][LIST_MENU_TEXT_LENGTH];  // Restaurant name
static char list_menu_sub_text[LIST_MENU_ROWS][LIST_MENU_SUB_TEXT_LENGTH];  // Direction and distance
static char list_menu_header_text[32] = "Restaurants";
static char query_result[32];
static char random_result[LIST_MENU_TEXT_LENGTH];
static int num_of_list_items;  // number of the returned items

// The main menu with "Random" and "List" options
static Window *s_main_window;
static MenuLayer *s_main_menu_layer;

// The random pick result
static Window *s_result_window;
static TextLayer *s_result_text_layer;

// The list menu for all candidate
static Window *s_list_window;
static MenuLayer *s_list_menu_layer;

// The waiting window
static Window *s_wait_window;
static TextLayer *s_wait_text_layer;

// Last query time
static time_t last_query_time;

// Is query on going?
static bool is_querying;

// The selected option while waiting
static int select_option;

static void send_query(void){
	DictionaryIterator *iter;
	app_message_outbox_begin(&iter);
	dict_write_uint8(iter, 0, 0);  // don't care about the content
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

static void main_menu_select_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context){
	time_t now;
	int index;

	APP_LOG(APP_LOG_LEVEL_INFO, "Select Click");

	select_option = cell_index->row;
	// Do we have the valid result?
	now = time(NULL);
	if(last_query_time > 0 && ((now - last_query_time) < RESULT_AGE_TIME)){
		// we have valid list, show them
		switch(cell_index->row){
			case 0:
				generate_random_result_window();
				break;
			case 1:
				window_stack_push(s_list_window, true);
				break;
			default:
				APP_LOG(APP_LOG_LEVEL_ERROR, "Unknown selection");
				break;
		}
		
	}
	else{
		// Either we didn't send query before, it is timeout, or we are still waiting for the result
		if(is_querying == false){
			send_query();
		}
		window_stack_push(s_wait_window, true);
	}

}

static void list_menu_select_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context){
// Currently do nothing. Consider to provide detailed information in the future.
	APP_LOG(APP_LOG_LEVEL_INFO, "%d item selected", cell_index->row);
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
}

static void wait_window_load(Window *window) {
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(window_layer);

	APP_LOG(APP_LOG_LEVEL_INFO, "Wait load");

	s_wait_text_layer = text_layer_create(bounds);
	text_layer_set_font(s_wait_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
	text_layer_set_text_alignment(s_wait_text_layer, GTextAlignmentCenter);
	text_layer_set_background_color(s_wait_text_layer, GColorClear);
	text_layer_set_text_color(s_wait_text_layer, GColorBlack);
	text_layer_set_text(s_wait_text_layer, "Waiting");
	layer_add_child(window_layer, text_layer_get_layer(s_wait_text_layer));
}

static void wait_window_unload(Window *window) {
	text_layer_destroy(s_wait_text_layer);
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
					// looks like the SDK has no strtok(). Cut it myself
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
			if(successFlag)
				window_stack_push(s_list_window, true);
			else
				generate_random_result_window();
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

	// Create main window
	s_main_window = window_create();
	window_set_window_handlers(s_main_window, (WindowHandlers){
		.load = main_window_load,
		.unload = main_window_unload,
	});

	// Create result window
	s_result_window = window_create();
	window_set_window_handlers(s_result_window, (WindowHandlers){
		.load = result_window_load,
		.unload = result_window_unload,
	});

	// Create list window
	s_list_window = window_create();
	window_set_window_handlers(s_list_window, (WindowHandlers){
		.load = list_window_load,
		.unload = list_window_unload,
	});

	// Create wait window
	s_wait_window = window_create();
	window_set_window_handlers(s_wait_window, (WindowHandlers){
		.load = wait_window_load,
		.unload = wait_window_unload,
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
}

int main(void) {
	init();
	app_event_loop();
	deinit();
}
