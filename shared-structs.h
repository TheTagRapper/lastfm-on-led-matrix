#ifndef IS_MAIN
#include <stdbool.h>
#include <stdlib.h>
#endif

#ifndef SHARED_STRUCTS
#define SHARED_STRUCTS

struct track_metadata {
	char track_name[255];
	char artist_name[255];
	char image_link[255];
	char image_request[300 * sizeof(char)];
	bool is_playing;
};

typedef struct TLS_CLIENT_T {
	struct altcp_pcb *pcb;
	bool connected;
	bool complete;
	int error;
	const char *http_request;
	int timeout;
	struct track_metadata* track_data;
} TLS_CLIENT_T;

#endif
