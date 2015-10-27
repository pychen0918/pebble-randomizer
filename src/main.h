#ifndef __MAIN_H__
#define __MAIN_H__

#include <pebble.h>
#include "query.h"

// Search behavior
#define SEARCH_RESULT_MAX_DATA_NUMBER	20	// Max number of returned data
#define SEARCH_RESULT_AGE_TIMEOUT	300 	// How long the list is valid in seconds

// Main Menu UI
#define MAIN_MENU_ROWS			3	// Random, list and settings
#define MAIN_MENU_TEXT_LENGTH		64
#ifdef PBL_ROUND
#define MAIN_BANNER_HEIGHT		44
#define MAIN_BANNER_TOP_MARGIN		12
#else
#define MAIN_BANNER_HEIGHT		32
#endif
#define NUMBER_OF_RANGE_VALUE		4	// for one unit, we have only 4 options
#define NUMBER_OF_UNIT_VALUE		2	// for unit, we have only 2 options

// What kind of operation user is currently performing
#define USER_OPERATION_RANDOM		0
#define USER_OPERATION_LIST		1
#define USER_OPERATION_SETTING		2
#define USER_OPERATION_DETAIL		3

// Useful macros
#define DATA_VALID			0
#define DATA_INVALID			1
#define MESSAGE_UID_INVALID		0
#define MESSAGE_UID_FIRST		1
#define MESSAGE_UID_MAX			255

// Default user settings
#define DEFAULT_SEARCH_RANGE		1	// 1km
#define DEFAULT_SEARCH_TYPE		1	// Restaurant
#define DEFAULT_SEARCH_OPENNOW		0	// do not add opennow filter
#define DEFAULT_SEARCH_PRICE		0	// don't care for price 
#define DEFAULT_SEARCH_UNIT		0	// meters

// Key for appmessage
#define KEY_STATUS			0
#define KEY_QUERY_TYPE			1	
#define KEY_QUERY_PLACE_ID		2	// store place id
#define KEY_QUERY_ERROR_MESSAGE		3
#define KEY_QUERY_UID			4	// unique id for each sent/received messages pair
#define KEY_QUERY_OPTION_RANGE		10
#define KEY_QUERY_OPTION_TYPE		11
#define KEY_QUERY_OPTION_OPENNOW	12
#define KEY_QUERY_OPTION_PRICE		13
#define KEY_DETAIL_ADDRESS		20	// detail information returned by 'detail' query
#define KEY_DETAIL_PHONE		21
#define KEY_DETAIL_RATING		22
#define KEY_LIST_FIRST			30	// Note: Must sync with js file manually. Key of the first restaurant information data

// Key for persist storage
#define PERSIST_KEY_USER_SETTING	40
#define PERSIST_KEY_STORAGE_VERSION	50	// We changed storage layout in 1.1, need version control

// Version string
#define APP_VERSION_STRING		"1.1"

// --------------------------------------------------------------------------------------
// Data Structure Defination
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
	uint8_t sorted_index[SEARCH_RESULT_MAX_DATA_NUMBER];			// The array that stores index of restaurant which sorted by distance
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
	uint8_t range;				// Index of selected range (0: 500m or 0.5mi, 1: 1km or 1mi, 2: 5km or 5mi, 3: 10km or 10mi)
	uint8_t type;				// Index of selected type (0: Food, 1: Restaurant, 2: Cafe, 3: Bar, 4: CVS, 5: Delivery, 6: Takeout)
	uint8_t opennow;			// Index of selected opennow filter (0: Disable, 1: Enable)
	uint8_t price;				// Index of selected price filter (0: Don't mind, 1: Inexpensive, 2: Moderate, 3: Expensive, 4: Very Expensive)
	uint8_t unit;				// Index of selected distance units (0: meters, 1: miles)
} UserSetting;

typedef struct __old_user_setting_t{
	uint8_t range;				// Index of selected range (0: 500m, 1: 1km, 2: 5km, 3: 10km)
	uint8_t type;				// Index of selected type (0: Food, 1: Restaurant, 2: Cafe, 3: Bar)
	uint8_t opennow;			// Index of selected opennow filter (0: Disable, 1: Enable)
} OldUserSetting;

typedef struct __menu_status_t{
	uint8_t user_operation;		 	// which function is currently performed by user (Random, List, Setting, Detail)
	uint8_t setting_menu_selected_option; 	// which setting menu is selected by user (Range, Keyword, Open Now)
	uint8_t user_detail_index;		// which restraunt user is looking for details
} MenuState;

// --------------------------------------------------------------------------------------
// Shared variables
// --------------------------------------------------------------------------------------

extern SearchResult search_result;	// The main data structure to store the returned search results
extern UserSetting user_setting;	// The main data structure to store custom searching configuations.
extern MenuState menu_state;		// The main data structure to store user's selection on the menu

extern GBitmap *icon_blank_bitmap;	// The blank icon for settings
extern GBitmap *icon_check_black_bitmap;// The black check icon for settings
extern GBitmap *icon_agenda_bitmap;	// The icon for action bar

extern GColor text_color;
extern GColor bg_color;
extern GColor highlight_text_color;
extern GColor highlight_bg_color;
extern GColor highlight_alt_text_color;
extern GColor highlight_alt_bg_color;
	
extern const char *main_menu_text[MAIN_MENU_ROWS];
extern const char *main_banner_text;
extern const char *setting_main_menu_header_text;
extern const char *wait_banner_text;

extern const char *query_status_error_message[QUERY_STATUS_NUM_OF_ERROR_TYPES];
extern const char *query_status_error_sub_message[QUERY_STATUS_NUM_OF_ERROR_TYPES];

extern const char *unknown_error_message;
extern const char *unknown_error_sub_message;

extern const char *setting_main_menu_text[5];		// type, range, opennow, price level, units
// meters: 500 M, 1 KM, 5 KM, 10 KM, miles: 0.5 miles, 1 miles, 5 miles, 10 miles
extern const char *setting_range_option_text[NUMBER_OF_UNIT_VALUE][NUMBER_OF_RANGE_VALUE];	
extern const char *setting_type_option_text[7];		// Food, Restaurant, Cafe, Bar, Convenience Store, Meal Delivery, Meal Takeaway
extern const char *setting_opennow_option_text[2];	// No, Yes
extern const char *setting_price_option_text[5];	// Don't care, Inexpensive, Moderate, Expensive, Very Expensive
extern const char *setting_unit_option_text[NUMBER_OF_UNIT_VALUE];	// meters, miles

extern const char *detail_address_text;
extern const char *detail_phone_text;
extern const char *detail_rating_text;
extern const char *detail_nodata_text;
extern const char *detail_star_text;

extern const char *direction_name[9];			// N, NE, E, ..., NW, N, total 9

// --------------------------------------------------------------------------------------
// Function prototypes
// --------------------------------------------------------------------------------------

// Check if current stored restaurant data is valid.
// Return TRUE if the stored data is valid, FALSE otherwise
bool validate_data(void);

// Get next uid. Note '0' is not a valid uid.
// It will update uid_next as well.
// Return an unsigned value between 1 and MESSAGE_UID_MAX
uint8_t get_next_uid(void);

// Get previous uid. Note '0' is not a valid uid
// Return an unsigned value between 1 and MESSAGE_UID_MAX
uint8_t get_previous_uid(void);

// allocate a space and copy string to that space. Return pointer to the space.
void *alloc_and_copy_string(char* string);

// search for certain store with place_id
int find_index_from_place_id(char *place_id);

// Free the dynamically allocated space in search_result
void free_search_result(void);

// Reset the sorted index
void reset_sorted_index(void);
#endif // #ifndef __MAIN_H__
