#include <pebble.h>
#include "main.h"
#include "query.h"

static int find_next_seperator(const char *string, const char seperator){
	int i=0;
	while(string[i]!='\0' && string[i]!=seperator){
		i++;
	}
	return i;
}

// Send out the 'list' query to retrieve 20 nearby restaurant info
void send_list_query(void){
	DictionaryIterator *iter;
	uint8_t range_index = user_setting.range + user_setting.unit*NUMBER_OF_RANGE_VALUE;

	app_message_outbox_begin(&iter);
	dict_write_uint8(iter, KEY_QUERY_TYPE, QUERY_TYPE_LIST);
	dict_write_uint8(iter, KEY_QUERY_UID, get_next_uid());
	dict_write_uint8(iter, KEY_QUERY_OPTION_RANGE, range_index);
	dict_write_uint8(iter, KEY_QUERY_OPTION_TYPE, user_setting.type);
	dict_write_uint8(iter, KEY_QUERY_OPTION_OPENNOW, user_setting.opennow);
	dict_write_uint8(iter, KEY_QUERY_OPTION_PRICE, user_setting.price);
	app_message_outbox_send();
}

// Send out the 'detail' query to retrieve certain restaurant detailed info
// Input is the index of the store in the list
void send_detail_query(uint8_t store_index){
	DictionaryIterator *iter;
	app_message_outbox_begin(&iter);
	dict_write_uint8(iter, KEY_QUERY_TYPE, QUERY_TYPE_DETAIL);
	dict_write_uint8(iter, KEY_QUERY_UID, get_next_uid());
	dict_write_cstring(iter, KEY_QUERY_PLACE_ID, search_result.restaurant_info[store_index].place_id);
	app_message_outbox_send();
}

// Parse the returned list dictionary data and store to search_result
int parse_list_message_handler(DictionaryIterator *iterator){
	Tuple *t = dict_read_first(iterator);
	int index, len, head;
	int ret = DATA_INVALID;
	char buf[256], temp_name[256], temp_place_id[256], temp_direction[4], temp_distance[16];
	char errmsg[256];

	APP_LOG(APP_LOG_LEVEL_DEBUG, "parse_list_message_handler");

	index = 0;
	while(t != NULL) {
		switch(t->key){
			case KEY_STATUS:
				search_result.query_status = t->value->uint8;
				break;
			case KEY_QUERY_ERROR_MESSAGE:
				strncpy(errmsg, t->value->cstring, sizeof(errmsg));
				break;
			case KEY_QUERY_TYPE:
			case KEY_QUERY_UID:
				// Simply ignore these keys, they are handled by receive callback
				break;
			default:
				//  Check if the key fall in the data range
				if((t->key >= KEY_LIST_FIRST) && (t->key < (KEY_LIST_FIRST + SEARCH_RESULT_MAX_DATA_NUMBER))){
					// value string format: Restaurant name|Direction|Distance|placeid
					strncpy(buf, t->value->cstring, sizeof(buf));
					len = head = 0;
					// name
					len = find_next_seperator(&buf[head], '|');
					strncpy(temp_name, &buf[head], len);
					temp_name[len] = '\0';
					// direction
					head = head + len + 1;
					len = find_next_seperator(&buf[head], '|');
					strncpy(temp_direction, &buf[head], len);
					temp_direction[len] = '\0';
					// distance
					head = head + len + 1;
					len = find_next_seperator(&buf[head], '|');
					strncpy(temp_distance, &buf[head], len);
					temp_distance[len] = '\0';
					// placeid
					head = head + len + 1;
					len = find_next_seperator(&buf[head], '|');
					strncpy(temp_place_id, &buf[head], len);
					temp_place_id[len] = '\0';
					// Assign value
					APP_LOG(APP_LOG_LEVEL_DEBUG, "%s %s %s %s", temp_name, temp_direction, temp_distance, temp_place_id);
					search_result.restaurant_info[index].name = alloc_and_copy_string(temp_name);
					search_result.restaurant_info[index].direction = (uint8_t)atoi(temp_direction);
					search_result.restaurant_info[index].distance = (uint16_t)atoi(temp_distance);
					search_result.restaurant_info[index].place_id = alloc_and_copy_string(temp_place_id);
					index++;
				}
				else
					APP_LOG(APP_LOG_LEVEL_ERROR, "Invalid key: %d", (int)t->key);
				break;
		}
		t = dict_read_next(iterator);
	}
	search_result.num_of_restaurant = index;

	// Update some information if query success
	if(search_result.query_status == QUERY_STATUS_SUCCESS){
		search_result.last_query_time = time(NULL);
		search_result.is_setting_changed = false;
		ret = DATA_VALID;
	}
	else if(search_result.query_status == QUERY_STATUS_GOOGLE_API_ERROR){
		search_result.api_error_message = alloc_and_copy_string(errmsg);
	}

	return ret;
}

// Parse the returned list dictionary data and store to search_result
int parse_detail_message_handler(DictionaryIterator *iterator){
	Tuple *t = dict_read_first(iterator);
	uint8_t rating = 0;
	char buf[2][256];
	char errmsg[256];
	int index = -1;
	int ret = DATA_INVALID;

	APP_LOG(APP_LOG_LEVEL_DEBUG, "parse_detail_message_handler");

	while(t!=NULL){
		switch(t->key){
			case KEY_STATUS:
				search_result.query_status = t->value->uint8;
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
			case KEY_QUERY_TYPE:
			case KEY_QUERY_UID:
				// Simply ignore these keys, they are handled by receive callback
				break;
			default:
				APP_LOG(APP_LOG_LEVEL_ERROR, "Invalid key: %d", (int)t->key);
				break;
		}
		t = dict_read_next(iterator);
	}

	// if we failed to find corresponding restaurant, show detail query fail message
	if(index < 0){
		search_result.query_status = QUERY_STATUS_NO_DETAIL;
	}

	// Store the information if everything is OK
	if(search_result.query_status == QUERY_STATUS_SUCCESS){
		search_result.restaurant_info[index].address = alloc_and_copy_string(buf[0]);
		search_result.restaurant_info[index].phone = alloc_and_copy_string(buf[1]);
		search_result.restaurant_info[index].rating = rating;
		ret = DATA_VALID;
	}
	else if(search_result.query_status == QUERY_STATUS_GOOGLE_API_ERROR){
		search_result.api_error_message = alloc_and_copy_string(errmsg);
	}
	return ret;
}

