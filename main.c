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
#include <cjson/cJSON.h>
#include "env.h"

static ip_addr_t true_ip;
static bool done = false;
static bool success = false;
static char *full_packet = NULL;

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
	printf("Enqueued Request: %s\n", state->http_request);

	
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
		printf("Connection Closed\n");
		return tls_client_close(state);
	}

	if (p->tot_len > 0)
	{
		// Copies whole buffer
		// Should replace later so only get JSON out so there is enough memory for pictures

		char buf[p->tot_len + 1];

		pbuf_copy_partial(p, buf, p->tot_len, 0);
		buf[p->tot_len] = 0;

		printf("\nNew Packet Received:\n %s, \n ", buf);

		// Concatenate into one string
		if (full_packet == NULL) full_packet = buff;
		else strcat(full_packet, buff);
		
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
	altcp_poll(state->pcb, tls_client_poll, 20 * 2);
	mbedtls_ssl_set_hostname(altcp_tls_context(state->pcb), "ws.audioscrobbler.com");

	altcp_connect(state->pcb, &true_ip, 443, tls_client_connected);
	return state;
}


int main(void)
{
	

	
}
