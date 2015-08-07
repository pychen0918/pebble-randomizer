#include <pebble.h>
#define MAIN_MENU_ROWS	2
#define LIST_MENU_ROWS  6

static char main_menu_text[MAIN_MENU_ROWS][16] = {"Random!","List"};
static char list_menu_text[LIST_MENU_ROWS][16] = {"First","Second", "Third", "Fourth", "Fifth", "Sixth"};

static char s_menu_text[16];

// The main menu with "Random" and "List" options
static Window *s_menu_window;
static MenuLayer *s_menu_layer;

// The random pick result
static Window *s_result_window;
static TextLayer *s_result_layer;

// The list menu for all candidate
static Window *s_list_window;
static MenuLayer *s_list_layer;

static uint16_t get_num_rows_callback(struct MenuLayer *menulayer, uint16_t section_index, void *callback_context){
	return MAIN_MENU_ROWS;
}

static void draw_row_handler(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *callback_context){
	char* text = main_menu_text[cell_index->row];

	APP_LOG(APP_LOG_LEVEL_INFO, "Draw row handler");
	snprintf(s_menu_text, sizeof(s_menu_text), "%s", text);

	menu_cell_basic_draw(ctx, cell_layer, text, NULL, NULL);
}

static void select_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context){

	APP_LOG(APP_LOG_LEVEL_INFO, "Select Click");
	switch(cell_index->row){
		case 0:
			window_stack_push(s_result_window, true);
			break;
		default:
			break;
	}
}

static void menu_window_load(Window *window) {
	// Create Window's child Layers here
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(window_layer);

	APP_LOG(APP_LOG_LEVEL_INFO, "Menu load");

	s_menu_layer = menu_layer_create(bounds);
	menu_layer_set_callbacks(s_menu_layer, NULL, (MenuLayerCallbacks){
		.get_num_rows = get_num_rows_callback,
		.draw_row = draw_row_handler,
		.select_click = select_callback
	});
	menu_layer_set_click_config_onto_window(s_menu_layer, window);
	layer_add_child(window_layer, menu_layer_get_layer(s_menu_layer));
}

static void menu_window_unload(Window *window) {
	menu_layer_destroy(s_menu_layer);
}

static void result_window_load(Window *window) {
	static char result[16];
	int index;
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(window_layer);
	
	APP_LOG(APP_LOG_LEVEL_INFO, "Result load");

	index = rand()%LIST_MENU_ROWS;
	strncpy(result, list_menu_text[index], sizeof(result));

	s_result_layer = text_layer_create(bounds);
	text_layer_set_font(s_result_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
	text_layer_set_text_alignment(s_result_layer, GTextAlignmentCenter);
	text_layer_set_background_color(s_result_layer, GColorClear);
	text_layer_set_text_color(s_result_layer, GColorBlack);
	text_layer_set_text(s_result_layer, result);
	layer_add_child(window_layer, text_layer_get_layer(s_result_layer));

	// Send app message for test!
	DictionaryIterator *iter;
	app_message_outbox_begin(&iter);
	// Add a key-value pair
	dict_write_uint8(iter, 0, 0);
	// Send the message!
	app_message_outbox_send();
}

static void result_window_unload(Window *window) {
	text_layer_destroy(s_result_layer);
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context){
	// Read first item
	Tuple *t = dict_read_first(iterator);

	APP_LOG(APP_LOG_LEVEL_INFO, "Message Received!");
	// For all items
	while(t != NULL) {
		APP_LOG(APP_LOG_LEVEL_INFO, "Name: %s", t->value->cstring);
		// Look for next item
		t = dict_read_next(iterator);
	}
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
	APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
	APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
	APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void init(){
	// Initialize random seed
	srand(time(NULL));

	// Create main window
	s_menu_window = window_create();
	window_set_window_handlers(s_menu_window, (WindowHandlers){
		.load = menu_window_load,
		.unload = menu_window_unload,
	});

	// Create result window
	s_result_window = window_create();
	window_set_window_handlers(s_result_window, (WindowHandlers){
		.load = result_window_load,
		.unload = result_window_unload,
	});

	// Register AppMessage callbacks
	app_message_register_inbox_received(inbox_received_callback);
	app_message_register_inbox_dropped(inbox_dropped_callback);
	app_message_register_outbox_failed(outbox_failed_callback);
	app_message_register_outbox_sent(outbox_sent_callback);

	// Open AppMessage
	app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());

	window_stack_push(s_menu_window, true);
}


static void deinit(){
	// Destoy main window
	window_destroy(s_menu_window);
}

int main(void) {
	init();
	app_event_loop();
	deinit();
}
