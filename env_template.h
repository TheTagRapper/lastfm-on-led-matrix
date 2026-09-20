#ifndef ENV_H
#define ENV_H

#define SSID "WIFI_NAME"
#define PASSWORD "WIFI_PASSWD"
#define LASTFM_HTTP_REQUEST "GET /2.0/?method=user.getrecenttracks&user=INSERT_TARGET_USER_HERE&api_key=INSERT_API_KEY_HERE&format=json&limit=1 HTTP/1.1\r\n" \
							"Host: ws.audioscrobbler.com\r\n" \
							"Connection: close\r\n" \
							"\r\n"
#endif
