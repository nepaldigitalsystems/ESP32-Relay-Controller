#ifndef _connect_h_
#define _connect_h_

#include "esp_err.h"

void wifi_init();
esp_err_t wifi_connect_sta(const char *SSID, const char *PASS, int timeout);
void wifi_connect_ap(const char *ssid);
void wifi_disconnect();

#endif