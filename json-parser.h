#include "cJSON.h"

#ifndef STDIO_INCLUDE
#include <stdio.h>
#endif

#ifndef IS_MAIN
#include "main.c"
#endif 

#ifndef LAST_FM_MSG_SIZE
#define LAST_FM_MSG_SIZE 3*1500*sizeof(char)
#endif

char json_string[];
char full_packet[];

struct track_metadata track_data;

size_t parse_json_buffer(char *buffer, size_t itemsize, size_t no_of_items, void* ignorethis)
{
	size_t bytes = itemsize * no_of_items;
	char *track_name;
	char *artist_name;
	
	// Parsing JSON
	cJSON *json = cJSON_Parse(buffer);
	if (json == NULL)
	{
		const char *error_ptr = cJSON_GetErrorPtr();
		if (error_ptr != NULL) 
		{
			printf("Error: %s  \n", error_ptr);
		}
		cJSON_Delete(json);
		return -1;
	}


	// Navigating down the tree to get 
	cJSON *recenttracks = cJSON_GetObjectItemCaseSensitive(json, "recenttracks");
	if (recenttracks == NULL)
	{
		printf("reccenttracks doesnt exist");
		return -1;
	}

	cJSON *tracks = cJSON_GetObjectItemCaseSensitive(recenttracks, "track");
	if (tracks == NULL)
	{
		printf("track doesnt exist");
		return -1;
	}

	cJSON *index = cJSON_GetArrayItem(tracks, 0);
	if (index == NULL)
	{
		printf("0 doesnt exist");
		return -1;
	}

	// Get Track Name
	cJSON *name = cJSON_GetObjectItemCaseSensitive(index, "name");
	if (name == NULL)
	{
		printf("track doesnt exist");
		return -1;
	}

	// Get Artist Name
	
	cJSON *artistObject = cJSON_GetObjectItemCaseSensitive(index, "artist");
	if (artistObject != NULL)
	{
		cJSON *artistText = cJSON_GetObjectItemCaseSensitive(artistObject, "#text");
		if (cJSON_IsString(artistText) && (artistText->valuestring != NULL))
		{
			 artist_name = artistText->valuestring; 
		} else { artist_name = "Unknown"; }
	} 
	else { artist_name = "Unknown"; }

	// Get whether it is playing
	int now_playing = 1;
	cJSON *attr = cJSON_GetObjectItemCaseSensitive(index, "@attr");
	if (attr == NULL) now_playing = 0;

	
	if (cJSON_IsString(name) && (name->valuestring != NULL))
	{
		track_name = name->valuestring;
	} 
	else 
	{
		printf("FAILURE? \n");
		cJSON_Delete(json);
	}

	track_data->track_name = track_name;
	track_data->artist_name = artist_name;
	track_data->is_playing = now_playing;
	
	if (now_playing)
	{
		printf("Now Playing: %s by %s", track_name, artist_name);

	} else
	{
		printf("Last Played Track: %s by %s", track_name, artist_name);
	}

	cJSON_Delete(json);
	return bytes;

}

void http_to_json(char[] )
{
	// full_packet and json_string are extern variables
	// full_packet stitched in lwip-callbacks
	// technically json_string can removed from extern but too lz rn

	int size_message = strlen(full_packet);
	int json_index = 0;
	//printf("\nNow Trying to Find JSON string\n");
	bool json_started = false;
					
	for (int i = 0; i < size_message; i++)
	{
		char starter_char = '{';
		//printf("%c", full_packet[i]);
		if (!json_started && full_packet[i] == '{')
		{
			json_started = true;
			json_string[json_index++] = full_packet[i];
		}
		else if (json_started)
		{
			json_string[json_index++] = full_packet[i];
		}
		
	}

	printf("Now Parsing JSON STRING : \n %s \n", json_string);

	parse_json_buffer(json_string, sizeof(char), strlen(json_string), NULL);
	
	// Reset Static Strings
	full_packet[0] = '\0';
	json_string[0] = '\0';
}
