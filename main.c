#include <stdio.h>
#define STDIO_INCLUDE

#define IS_MAIN

#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "cJSON.h"
#include "env.h"

#include "json-parser.h"
#include "lwip-callbacks.h"


#ifndef SHARED_STRUCTS
#define SHARED_STRUCTS
#include "shared-structs.h"
#endif

ip_addr_t metadata_ip;
ip_addr_t image_ip;
bool done = false;
bool success = false;



int main(void)
{
	// Initiate input
	stdio_init_all();
	sleep_ms(2000);

	printf("I/O Initialized\n");
	// Initiate cyw43
	cyw43_arch_init();

	// Enable Wifi Mode
	cyw43_arch_enable_sta_mode();
	printf("Wifi Enabled\n");
	// Try to connect to wifi
	if (cyw43_arch_wifi_connect_timeout_ms(SSID, PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 10000))
	{
		printf("Failed to connect to WiFi\n");
		fail_state();
	}

	// Initalise struct for metadata
	struct track_metadata* track_data = malloc(255 * sizeof(char) + 255 * sizeof(char) + sizeof(bool));
	track_data->image_link[0] = 0;


	// Connection sucessful - Find IP
	cyw43_arch_lwip_begin();
	err_t err_metadata = dns_gethostbyname("ws.audioscrobbler.com", &metadata_ip, my_dns_found_callback, NULL);
	cyw43_arch_lwip_end();

	// Keep going until IP is resolved
	if (err_metadata == ERR_INPROGRESS)
	{
		// This is for resolving ip
		while (!done)
		{
			cyw43_arch_poll();
			sleep_ms(10);
		}
		// IP resolved
		if (success)
		{
			printf("audioscrobbler resolved to: %s\n", ipaddr_ntoa(&metadata_ip));

			// No Certificate
			printf("Now requesting metadata\n");
			TLS_CLIENT_T* json_state = tls_client_setup(track_data, NULL);
						
			while (!json_state->complete)
			{
				cyw43_arch_poll();
				cyw43_arch_wait_for_work_until(make_timeout_time_ms(1000));

			}

		
		}
	
	}
	else if (err_metadata == ERR_OK)
	{
		printf("Found Metadata IP: %s\n", ipaddr_ntoa(&metadata_ip));
	}

	cyw43_arch_lwip_begin();
	err_t err_image = dns_gethostbyname("lastfm.freetls.fastly.net", &image_ip, my_dns_found_callback, NULL);
	cyw43_arch_lwip_end();

	if (err_image == ERR_INPROGRESS)
	{
		// Set finding IP variables false
		done = false;
		success = false;
		while (!done)
		{
			cyw43_arch_poll();
			sleep_ms(10);
		}

		if (success)
		{
			printf("Found Image IP: %s\n", ipaddr_ntoa(&image_ip));

			// Requesting image now
			printf("\n\nRequesting Image Now\n%s\n", track_data->image_request);
			TLS_CLIENT_T* image_state = tls_client_setup(track_data, track_data->image_request);
			while (!image_state->complete)
			{
				cyw43_arch_poll();
				cyw43_arch_wait_for_work_until(make_timeout_time_ms(1000));
												
			}
		}
	}

	free(track_data);
	while (true) sleep_ms(1000);		

	
}
