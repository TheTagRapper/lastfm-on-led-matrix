#include <stdio.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <cjson/cJSON.h>

#include "env.h"

size_t got_data(char *buffer, size_t itemsize, size_t no_of_items, void* ignorethis)
{
	size_t bytes = itemsize * no_of_items;

	
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

	cJSON *name = cJSON_GetObjectItemCaseSensitive(index, "name");
	if (name == NULL)
	{
		printf("track doesnt exist");
		return -1;
	}
		
	if (cJSON_IsString(name) && (name->valuestring != NULL))
	{
		printf("Name: %s \n", name->valuestring);
	} else printf("FAILURE? \n");
	cJSON_Delete(json);
	return bytes;

}


int main(void)
{
	CURL *curl;
	CURLcode result;

	curl = curl_easy_init();
	
	if (curl == NULL)
	{
		fprintf(stderr, "init failed\n");
		return -1;
	}

	// Setting request destination
	curl_easy_setopt(curl, CURLOPT_URL, REQUEST_URL);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, got_data); // Call function


	// Doing the request
	result = curl_easy_perform(curl);

	// Printing out error
	if (result != CURLE_OK)
	{
		fprintf(stderr, "Error: %s\n", curl_easy_strerror(result));
		return -1;
	}

	curl_easy_cleanup(curl);

	return 0;
}
