#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "cJSON.h"
#include "env.h"

#include "json-parser.h"
#include "lwip-callbacks.h"

ip_addr_t true_ip;
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

	// Connection sucessful - Find IP
	cyw43_arch_lwip_begin();
	err_t err = dns_gethostbyname("ws.audioscrobbler.com", &true_ip, my_dns_found_callback, NULL);
	cyw43_arch_lwip_end();

	// Keep going until IP is resolved
	if (err == ERR_INPROGRESS)
	{
		while (!done)
		{
			cyw43_arch_poll();
			sleep_ms(10);
		}
		// IP resolved
		if (success)
		{
			printf("Resolved to: %s\n", ipaddr_ntoa(&true_ip));
			// No Certificate
			TLS_CLIENT_T* state = tls_client_setup();
			
			while (!state->complete)
			{
				cyw43_arch_poll();
				cyw43_arch_wait_for_work_until(make_timeout_time_ms(1000));

			}

		
		}
	
	}
	else if (err == ERR_OK)
	{
		printf("Found %s\n", ipaddr_ntoa(&true_ip));
	}

	while (true) sleep_ms(1000);		

	
}
