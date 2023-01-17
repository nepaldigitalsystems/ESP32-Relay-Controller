#ifndef _server_h_
#define _server_h_

#include "esp_err.h"
#include "esp_http_server.h"

void http_server_ap_mode(void);
void http_server_sta_mode(void);
void Connect_Portal(void);
void start_dns_server(void);

esp_err_t login_auth_handler(httpd_req_t *req);
esp_err_t AP_TO_STA(httpd_req_t *req);
esp_err_t assets_handler(httpd_req_t *req);
esp_err_t img_handler(httpd_req_t *req);
esp_err_t login_handler(httpd_req_t *req);
esp_err_t connect_ssid(httpd_req_t *req);

#endif