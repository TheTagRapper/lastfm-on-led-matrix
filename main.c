#include <stdio.h>
#include <curl/curl.h>
#include <stdlib.h>
#include "env.h"

size_t got_data(char *buffer, size_t itemsize, size_t no_of_items, void* ignorethis)
{
	size_t bytes = itemsize * no_of_items;

	printf("New Chunk (%zu bytes)\n", bytes);
	printf("Buffer: %s\n", buffer);
	
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
