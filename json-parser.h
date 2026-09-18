#include "cJSON.h"

#ifndef STDIO_INCLUDE
#include <stdio.h>
#endif

#ifndef IS_MAIN
#include "main.c"
#endif 

#ifndef LAST_FM_MSG_SIZE
#define LAST_FM_MSG_SIZE 3*1500
#endif

#ifndef STRUCTS_INCLUDED
#include "shared-structs.h"
#define STRUCTS_INCLUDED
#endif

int parse_json_buffer(char *buffer, struct track_metadata* track_data)
{

	char *track_name;
	char *artist_name;
	char *image_link;
	
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

	cJSON *image_array = cJSON_GetObjectItemCaseSensitive(index, "image");
	if (image_array == NULL)
	{
		printf("Images don't exist");
		return -1;
	}

	cJSON *image_small_data = cJSON_GetArrayItem(image_array, 0);
	if (image_small_data == NULL)
	{
		printf("small image doesn't exist");
		return -1;
	}

	cJSON *image_small_link = cJSON_GetObjectItemCaseSensitive(image_small_data, "#text");
	if (image_small_link == NULL)
	{
		printf("Link for small image doesn't exist");
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
		printf("Track Name has not been found\n");
		cJSON_Delete(json);
	}

	if (cJSON_IsString(image_small_link) && image_small_link->valuestring != NULL)
	{
		image_link = image_small_link->valuestring;
	}
	else
	{
		printf("Small Image Link not found \n");
	}

	strcpy(track_data->track_name, track_name);
	strcpy(track_data->artist_name, artist_name);
	strcpy(track_data->image_link, image_link);	
	track_data->is_playing = now_playing;
	
	if (now_playing)
	{
		printf("Now Playing: %s by %s\n", track_name, artist_name);

	} else
	{
		printf("Last Played Track: %s by %s\n", track_name, artist_name);
	}

	printf("Small Image Link Is: %s\n", image_link);

	//

	cJSON_Delete(json);
	
}

void http_to_json(char* full_packet, char* json_string, int string_size)
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

	printf("Now Parsing JSON STRING : \n %s ", json_string);	

}

void image_link_to_request(char *image_link, char *request, int link_length, int request_length)
{
	// https:\/\/lastfm-img.freetls.fastly.net\/i\/u\/34s\/918b8de2341bd849b6c9e54a107380b6.jpg
	// Hostname : lastfm-img.freetls.fastly.net
	// i/u/34s

	bool start_copying = false;
	bool end_of_string = false;
	
	// will stop three characters after a dot
	// This stopping conditioner only triggers after start_copying is true

	int image_link_index = 0;
	int image_name_index = 0;

	char buff_b4_link[3] = "___";
	char image_name[35];

	
	while (image_link_index < link_length-1 && !end_of_string)
	{
		// For some reason accessing buffer as string messes it up
		// Access each element individually
		bool is_jpg = buff_b4_link[0] == 'j' && buff_b4_link[1] == 'p' && buff_b4_link[2] == 'g';
		bool is_png = buff_b4_link[0] == 'p' && buff_b4_link[1] == 'n' && buff_b4_link[2] == 'g';
		if (start_copying)
		{
			if (!is_jpg && !is_png) image_name[image_name_index++] = image_link[image_link_index]; 
			else
			{
				end_of_string = true;
				image_name[image_name_index] = '\0';	
			} 
		}
		buff_b4_link[0] = buff_b4_link[1];
		buff_b4_link[1] = buff_b4_link[2];
		buff_b4_link[2] = image_link[image_link_index];

		// Check until we know we have reached the image name
		// Everything before is the same
		if (buff_b4_link[0] == '4' && buff_b4_link[1] == 's' && buff_b4_link[2] == '/') {start_copying = true; printf("STARTED COPYING\n");}
		
		image_link_index++;		
		printf("\nBUFF AT END OF LOOP: %c | %c | %c \n", buff_b4_link[0], buff_b4_link[1], buff_b4_link[2]);
	}
	printf("Image name is : %s", image_name);
	// Check if condition has been reached
	 snprintf(request, request_length+200 ,"GET /i/u/34s/%s HTTP/1.1\r\nHost: lastfm.freetls.fastly.net\r\nConnection: close\r\n\r\n", image_name);

}


