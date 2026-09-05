#include "cJSON.h"
#ifndef STDIO_INCLUDE
#include <stdio.h>
#endif

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
