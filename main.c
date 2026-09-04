#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/dns.h"
#include "lwip/ip_addr.h"
#include "lwip/pbuf.h"
#include "lwip/altcp_tcp.h"
#include "lwip/altcp_tls.h"
#include "mbedtls/ssl.h"
#include "cJSON.h"
#include "env.h"

static ip_addr_t true_ip;
static bool done = false;
static bool success = false;
static char full_packet[2*1500*sizeof(char)] = "";
static char json_string[2*1500*sizeof(char)] = "";

typedef struct TLS_CLIENT_T {
	struct altcp_pcb *pcb;
	bool connected;
	bool complete;
	int error;
	const char *http_request;
	int timeout;
} TLS_CLIENT_T;

static struct altcp_tls_config *tls_config = NULL;

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



static err_t tls_client_close(void *arg) {
	TLS_CLIENT_T *state = (TLS_CLIENT_T*)arg;
	err_t err = ERR_OK;

	state->complete = true;
	if (state->pcb != NULL)
	{
		altcp_arg(state->pcb, NULL);
        altcp_poll(state->pcb, NULL, 0);
        altcp_recv(state->pcb, NULL);
        altcp_err(state->pcb, NULL);
        err = altcp_close(state->pcb);
        if (err != ERR_OK) {
            printf("close failed %d, calling abort\n", err);
            altcp_abort(state->pcb);
            err = ERR_ABRT;
        }
        state->pcb = NULL;
    }
    return err;
}

static err_t tls_client_connected(void *arg, struct altcp_pcb *pcb, err_t err)
{
	TLS_CLIENT_T *state = (TLS_CLIENT_T*)arg;
	if (err != ERR_OK) {
		printf("Connection Failed %d\n", err);
		return tls_client_close(state);
	}

	printf("Connected to Server\n");
	state->connected = true;
	
	err = altcp_write(state->pcb, state->http_request, strlen(state->http_request), TCP_WRITE_FLAG_COPY);
	if (err != ERR_OK)
	{
		printf("error writing data, err=%d", err);
		return tls_client_close(state);
	}
	printf("Enqueued Request");
	//printf(": %s\n", state->http_request);

	
	err = altcp_output(state->pcb);
	if (err != ERR_OK)
	{
		printf("error sending  data, err=%d", err);
		return tls_client_close(state);
	}
	printf("Outputted Request\n");
	
	return ERR_OK;
}


void my_dns_found_callback(const char *name, const ip_addr_t *ipaddr, void *arg)
{
	if (ipaddr == NULL)
	{
		printf("Resolution Failed\n");
		done = true;
		return ;
	}

	true_ip = *ipaddr;
	done = true;
	success = true;	
}

static TLS_CLIENT_T* tls_client_init(void) {
    TLS_CLIENT_T *state = calloc(1, sizeof(TLS_CLIENT_T));
    if (!state) {
        printf("failed to allocate state\n");
        return NULL;
    }

    return state;
}


static err_t tls_client_poll(void *arg, struct altcp_pcb *pcb) {
	printf("\nPOLL\n");
	TLS_CLIENT_T *state = (TLS_CLIENT_T*)arg;

	if (state->connected) return ERR_OK;

	printf("timed out\n");
	state->error = PICO_ERROR_TIMEOUT;
	return tls_client_close(arg);
}

static void tls_client_err(void *arg, err_t err)
{
	TLS_CLIENT_T *state = (TLS_CLIENT_T*)arg;
	printf("tls_client_err %d", err);
	tls_client_close(state);
	state->error = PICO_ERROR_GENERIC;
}

static err_t tls_client_recv(void *arg, struct altcp_pcb *pcb, struct pbuf *p, err_t) {
	TLS_CLIENT_T *state = (TLS_CLIENT_T*) arg;

	// pbuf is packet and needs to be freed later
	if (!p)
	{
		printf("\nConnection Closed\n");
		//printf("Whole Message: \n %s \n \n", full_packet);

		int size_message = strlen(full_packet);
		int json_index = 0;
		if (size_message > 0)
		{
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
		
		
		return tls_client_close(state);
	}

	if (p->tot_len > 0)
	{
		// Copies whole buffer
		// Should replace later so only get JSON out so there is enough memory for pictures

		char buf[p->tot_len + 1];

		pbuf_copy_partial(p, buf, p->tot_len, 0);
		buf[p->tot_len] = 0;

		//printf("\nNew Packet Received\n %s, \n ", buf);

		//Concatenate into one string
		if (strlen(full_packet) == 0) strcat(full_packet, buf);
		else strcat(full_packet, buf);
		//printf("CONCATENATION OCCURRED\n");
		
		// Confirms we have processed the data
		altcp_recved(pcb, p->tot_len);

	}
	pbuf_free(p);
	return ERR_OK;
}

void fail_state()
{
	printf("In eternal fail state\n");
	while (1) sleep_ms(1000);
}

static TLS_CLIENT_T* tls_client_setup()
{
	struct altcp_tls_config *tls_config = altcp_tls_create_config_client(NULL, 0);
	
	TLS_CLIENT_T *state = tls_client_init();
	if (!state)
	{
		printf("failed to assign state");
		fail_state();
	}
	state->pcb = altcp_tls_new(tls_config, IPADDR_TYPE_ANY);
	if (!state->pcb) { printf("failed to create pcb\n"); return NULL; }

	state->http_request = LASTFM_HTTP_REQUEST;
	state->timeout = 15;

	altcp_arg(state->pcb, state);
	altcp_recv(state->pcb, tls_client_recv);
	altcp_err(state->pcb, tls_client_err);
	altcp_poll(state->pcb, tls_client_poll, 5*2);
	mbedtls_ssl_set_hostname(altcp_tls_context(state->pcb), "ws.audioscrobbler.com");

	altcp_connect(state->pcb, &true_ip, 443, tls_client_connected);
	return state;
}


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
