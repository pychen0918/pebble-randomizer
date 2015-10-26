#ifndef __QUERY_H__
#define __QUERY_H__

#include <pebble.h>

// If changed the following status code, the js file might need to be updated
// Also need to check corresponding const error message strings
#define QUERY_STATUS_SUCCESS		0
#define QUERY_STATUS_NO_RESULT		1
#define QUERY_STATUS_GPS_TIMEOUT	2
#define QUERY_STATUS_GOOGLE_API_ERROR	3
#define QUERY_STATUS_NO_DETAIL		4
#define QUERY_STATUS_NUM_OF_ERROR_TYPES 5

#define QUERY_TYPE_LIST			0	// ask for the information 20 nearby store
#define QUERY_TYPE_DETAIL		1	// ask for the information of one certain store
#define QUERY_TYPE_INVALID		2

int parse_list_message_handler(DictionaryIterator *iterator);
int parse_detail_message_handler(DictionaryIterator *iterator);
void send_list_query(void);
void send_detail_query(uint8_t store_index);

#endif // #ifndef __QUERY_H__
