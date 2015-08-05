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
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(window_layer);
	
	APP_LOG(APP_LOG_LEVEL_INFO, "Result load");

	s_result_layer = text_layer_create(bounds);
	text_layer_set_font(s_result_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
	text_layer_set_text_alignment(s_result_layer, GTextAlignmentCenter);
	text_layer_set_background_color(s_result_layer, GColorClear);
	text_layer_set_text_color(s_result_layer, GColorBlack);
	text_layer_set_text(s_result_layer, "Result");
	layer_add_child(window_layer, text_layer_get_layer(s_result_layer));
}

static void result_window_unload(Window *window) {
	text_layer_destroy(s_result_layer);
}

static void init(){
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
