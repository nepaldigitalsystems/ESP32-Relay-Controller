#ifndef _server_h_
#define _server_h_

#include "esp_err.h"
#include "esp_http_server.h"

void http_server_ap_mode(void);
void http_server_sta_mode(void);
void Connect_Portal(void);
void start_dns_server(void);
void wifi_disconnect();
// void start_MDNS(void);

#endif