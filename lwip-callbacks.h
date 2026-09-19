#include "lwip/dns.h"
#include "lwip/ip_addr.h"
#include "lwip/pbuf.h"
#include "lwip/altcp_tcp.h"
#include "lwip/altcp_tls.h"
#include "mbedtls/ssl.h"

#ifndef STRUCTS_INCLUDED
#include "shared-structs.h"
#define STRUCTS_INCLUDED
#endif

#define LAST_FM_MSG_SIZE 3*1500

static char *full_packet;
int full_packet_length = 0;
static char *json_string;

extern bool done;
extern bool success;
extern ip_addr_t metadata_ip;
extern ip_addr_t image_ip;

static struct altcp_tls_config *tls_config = NULL;

static err_t tls_client_close(void *arg) {

	TLS_CLIENT_T *state = (TLS_CLIENT_T*)arg;
	err_t err = ERR_OK;

	// CLIENT_T will now end
	state->complete = true;
	
	if (state->pcb != NULL)
	{
		// Resetting PCB
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

	// Enqueueing Request
	err = altcp_write(state->pcb, state->http_request, strlen(state->http_request), TCP_WRITE_FLAG_COPY);
	if (err != ERR_OK)
	{
		printf("error writing data, err=%d", err);
		return tls_client_close(state);
	}
	printf("Enqueued Request\n");
	//printf(": %s\n", state->http_request);

	// Sending Request
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

	if (ip_addr_isany(&metadata_ip))
	{
		metadata_ip = *ipaddr;
		ip_addr_set_zero(&image_ip);
	} else
	{
		ip_addr_set_zero(&metadata_ip);
		image_ip = *ipaddr;
	}

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

		if (state->track_data->image_link[0] == 0)
		{
			if (strlen(full_packet) > 0) http_to_json(full_packet, json_string, LAST_FM_MSG_SIZE);
			full_packet[0] = 0;
			full_packet_length = 0;

			parse_json_buffer(json_string, state->track_data);
			json_string[0] = 0;

			char* image_request = calloc(150, sizeof(char));
			
			image_link_to_request(state->track_data->image_link, image_request, 200, 300);
			printf("Image is now turned to request: %s\n", image_request);
			strcpy(state->track_data->image_request, image_request);
			free(image_request);
		} else
		{
			printf("\nImage Received:\n%s");

			// Debugging full packet
			// Cannot be interpreted as string as it contains 0x00 which null terminates
			bool start_printing = false;
			for (int i = 0;  i < full_packet_length; i++ )
			{
				if (full_packet[i] == 255) start_printing = true;
				if (start_printing)
				{
					printf("%X", full_packet[i]);
					if (i+1 % 2 == 0) printf(" ");
					if (i+1 % 16 == 0) printf("\n");
				}
			}
			full_packet[0] = 0;
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
		// Can't use strcat as image contains null termination 0x00
		for (int i =  0; i < p->tot_len; i++)
		{
			if (full_packet_length+i < LAST_FM_MSG_SIZE)
			{
				full_packet[full_packet_length + i] = buf[i];
			}
			// NEED TO RETURN ERROR IF TOO BIG
		}
		full_packet_length += p->tot_len;

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

static TLS_CLIENT_T* tls_client_setup(struct track_metadata* track_data, void *request_link_arg)
{
	struct altcp_tls_config *tls_config = altcp_tls_create_config_client(NULL, 0);

	full_packet = (char *)calloc(LAST_FM_MSG_SIZE, sizeof(char));
	json_string = (char *)calloc(LAST_FM_MSG_SIZE, sizeof(char));

	char *request_link;
	printf("\nrequest_link_arg : %s", request_link_arg);
	if (request_link_arg == NULL)
	{
		request_link = LASTFM_HTTP_REQUEST;
	}
	else
	{
		request_link = (char *)request_link_arg;
	}
	
	TLS_CLIENT_T *state = tls_client_init();
	if (!state)
	{
		printf("failed to assign state");
		fail_state();
	}
	
	state->pcb = altcp_tls_new(tls_config, IPADDR_TYPE_ANY);
	if (!state->pcb) { printf("failed to create pcb\n"); return NULL; }

	state->http_request = request_link;
	state->timeout = 15;
	state->track_data = track_data;


	char *domain;
	if (request_link_arg)
	{
		domain = "lastfm.freetls.fastly.net";
	}
	else
	{
		domain = "ws.audioscrobbler.com";
	}

	printf("\n\nDomain: %s\n\n Request: %s\n", domain, state->http_request);
	
	altcp_arg(state->pcb, state);
	altcp_recv(state->pcb, tls_client_recv);
	altcp_err(state->pcb, tls_client_err);
	altcp_poll(state->pcb, tls_client_poll, 5*2);
	mbedtls_ssl_set_hostname(altcp_tls_context(state->pcb), domain);

	if (!request_link_arg)
	{
		altcp_connect(state->pcb, &metadata_ip, 443, tls_client_connected);
	}
	else
	{
		altcp_connect(state->pcb, &image_ip, 443, tls_client_connected);
	}
	return state;
}
