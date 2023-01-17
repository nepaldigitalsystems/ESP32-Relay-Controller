#include <stdio.h>
#include "DATA_FIELD.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_spiffs.h"
#include "toggleled.h"
#include "cJSON.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "connect.h"
#include "dns_hijack_srv.h"
#include "mdns.h"

// #define AMX_APs 10 // scans max = 10 AP
httpd_handle_t server = NULL; // for server.c only
const char *local_server_name = "esp";
bool dashboard_inUse = false;

/*countdown variable*/
#define Unlock_duration 180 // sec
uint8_t count_sec = Unlock_duration;
StaticTimer_t static_timer;
TimerHandle_t xTimer = NULL;
BaseType_t timer_status = pdFAIL;

/*extern functions*/
extern esp_err_t wifi_connect_sta(const char *SSID, const char *PASS, int timeout);
extern void wifi_connect_ap(const char *SSID);
extern esp_err_t login_cred(auth_t *Data);

void http_server_ap_mode(void);
void http_server_sta_mode(void);

void CountDown_timer(TimerHandle_t xTimer) // service routine for rtos-timer
{
    if (count_sec > 0)
    {
        count_sec--;
    }
    else
    {
        count_sec = Unlock_duration; // refresh the minute counter
        response.approve = false;
        timer_status = pdFAIL;
        xTimerStop(xTimer, 0);
        dashboard_inUse = false;
        ESP_LOGE("Login_Timeout", "Login_status : %s", (response.approve) ? "Unlocked" : "Locked");
    }
    ESP_LOGE("DASHBOARD_STATUS", "%s", (dashboard_inUse) ? "InUse" : "Not InUse");
}

static esp_err_t file_open(char path[], httpd_req_t *req)
{
    esp_vfs_spiffs_conf_t esp_vfs_spiffs_conf =
        {
            .base_path = "/spiffs",
            .partition_label = NULL,
            .max_files = 5,
            .format_if_mount_failed = true};
    esp_vfs_spiffs_register(&esp_vfs_spiffs_conf);
    FILE *file = fopen(path, "r");
    if (file == NULL)
    {
        httpd_resp_send_404(req);        // file not found
        esp_vfs_spiffs_unregister(NULL); // avoid memory leaks
        return ESP_OK;
    }
    char lineRead[256];
    while (fgets(lineRead, sizeof(lineRead), file))
    {
        httpd_resp_sendstr_chunk(req, lineRead);
    }
    httpd_resp_sendstr_chunk(req, NULL);
    fclose(file);
    esp_vfs_spiffs_unregister(NULL); // unmount
    return ESP_OK;
}

/***************** AUTH POST HANDLE  *************************************/
// 3. write the handler for the registered URL
esp_err_t dashboard_handler(httpd_req_t *req) // generally we dont want other file to see this
{
    ESP_LOGI("ESP_SERVER", "URL:- %s", req->uri); // display the URL
    if (xTimer == NULL)
        xTimer = xTimerCreateStatic("MY_timer", pdMS_TO_TICKS(1000), pdTRUE, NULL, CountDown_timer, &static_timer);

    ESP_LOGW("Login_Timeout", "Login_status : %s ....", (response.approve) ? "Unlocked" : "Locked");
    if (response.approve && (dashboard_inUse == false)) // approve = 1 -> Login_timeout = False
    {
        count_sec = Unlock_duration;
        if (timer_status != pdTRUE)
        {
            timer_status = xTimerStart(xTimer, 0); // timer_status => pdTrue
        }
        dashboard_inUse = true;
        ESP_LOGE("DASHBOARD_STATUS", "%s", (dashboard_inUse) ? "InUse" : "Not InUse");
        ESP_LOGW("Login_Timeout", "\t\t\t.....Time left = %d sec", count_sec);
        return file_open("/spiffs/dashboard.html", req); // Also, can send other response values from here
    }
    else
    {
        /*generate an alert from "dashboard.js" when reloading in locked status (response => 302)*/
        httpd_resp_set_status(req, "302 FOUND");
        httpd_resp_set_hdr(req, "location", "/");
        file_open("/spiffs/nds.html", req);
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }
}
esp_err_t login_handler(httpd_req_t *req) // generally we dont want other file to see this
{
    ESP_LOGI("ESP_SERVER", "URL:- %s", req->uri); // display the URL
    ESP_LOGE("DASHBOARD_STATUS", "%s", (dashboard_inUse) ? "DashBoard currently in use! Please try again after 3min" : "Free to Use! You can login to access it.");
    dashboard_inUse = false;
    return file_open("/spiffs/nds.html", req);
}
esp_err_t connect_ssid(httpd_req_t *req) // generally we dont want other file to see this
{
    ESP_LOGI("ESP_SERVER", "URL:- %s", req->uri);
    return file_open("/spiffs/wifi_connect.html", req);
}
esp_err_t captive_handler(httpd_req_t *req) // generally we dont want other file to see this
{
    ESP_LOGI("ESP_SERVER", "URL:- %s", req->uri);
    char host[32];
    esp_err_t ret = httpd_req_get_hdr_value_str(req, "Host", host, sizeof(host) - 1);
    ESP_LOGE("HOST_TAG", "Incoming header : %s", host);
    // else if (strcmp(host, "connectivitycheck.gstatic.com") == 0)
    // else if (strcmp(host, "connectivitycheck.android.com") == 0)
    // else if (strcmp(host, "connectivity-check.ubuntu.com") == 0)
    // else if (strcmp(host, "www.msftconnecttest.com") == 0)
    // else if (strcmp(host, "connect.rom.miui.com") == 0)
    // else if (strcmp(host, "dns.weixin.qq.com") == 0)
    // else if (strcmp(host, "nmcheck.gnome.org") == 0)
    if ((ret == ESP_OK) && (host != NULL))
    {
        file_open("/spiffs/captive.html", req);
        httpd_resp_send(req, NULL, 0);
    }
    else
    {
        ESP_LOGE("HOST_TAG", "Error getting Host header (%s): (%s)", esp_err_to_name(ret), host);
    }
    return ESP_OK;
}

esp_err_t login_auth_handler(httpd_req_t *req) // invoked when login_post is activated
{
    ESP_LOGI("ESP_SERVER", "URL:- %s", req->uri); // display the URL
    static auth_t cred;                           // cred obj // store it into the memory even after this function terminates
    char buffer[100];                             // this stores username and password into buffer //
    memset(&buffer, 0, sizeof(buffer));
    httpd_req_recv(req, buffer, req->content_len); // size_t recv_size = (req->content_len) > sizeof(buffer) ? sizeof(buffer) : (req->content_len);
    ESP_LOGW("login_data_post", "Buffer : %s", buffer);

    // get the json structured credentials
    if (strstr(buffer, ":") != NULL)
    {
        cJSON *payload = cJSON_Parse(buffer); // returns an object holding respective [ key:value pair data ]
        strcpy(cred.username, cJSON_GetObjectItem(payload, "username")->valuestring);
        strcpy(cred.password, cJSON_GetObjectItem(payload, "password")->valuestring);
        ESP_LOGI("Parse_tag", "USERNAME = %s", cred.username);
        ESP_LOGI("Parse_tag", "PASSWORD = %s", cred.password);
        cJSON_Delete(payload);
    }
    else
    {
        // manual parsing
        char *U = strtok(buffer, "\r\n");
        char *P = strtok(NULL, "\r\n");
        U = strchr(U, '=') + 1;          // RIKESH
        P = strchr(P, '=') + 1;          // PASS
        sprintf(cred.username, "%s", U); //
        sprintf(cred.password, "%s", P);
        ESP_LOGE("Parse_tag", "%s", cred.username);
        ESP_LOGE("Parse_tag", "%s", cred.password);
    }

    // authentication of username and password
    if ((strcmp(cred.username, "RIKESH") == 0) && (strcmp(cred.password, "PASS") == 0))
    {
        response.approve = true;
        // retrieve the username and password from internal nvs storage and compare if exists
        if (login_cred(&cred) == ESP_OK) // when internal empty => return ok ;
        {
            nvs_handle_t nvs;
            nvs_open("loginCreds", NVS_READWRITE, &nvs); // namespace => loginCreds [creates ]
            nvs_set_str(nvs, "username", cred.username);
            nvs_commit(nvs);
            nvs_set_str(nvs, "password", cred.password);
            nvs_commit(nvs);
            nvs_close(nvs);
            response.write = true;
        }
    }
    else
    {
        ESP_LOGE("AUTH_TAG", "Please reload into same IP. Incorrect username/password");
        response.approve = false;
        response.write = false;
        httpd_resp_set_status(req, "204 NO CONTENT");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }
    // sending json data
    cJSON *JSON_data = cJSON_CreateObject();
    cJSON_AddNumberToObject(JSON_data, "approve", (uint8_t)response.approve);
    cJSON_AddNumberToObject(JSON_data, "write", (uint8_t)response.write);
    char *string_json = cJSON_Print(JSON_data);
    ESP_LOGE("JSON_REPLY", "%s", string_json);

    httpd_resp_set_type(req, "application/json"); // sending json data as response
    httpd_resp_sendstr(req, string_json);         // chunk
    httpd_resp_send(req, NULL, 0);

    free(string_json);
    cJSON_free(JSON_data);

    return ESP_OK;
}

esp_err_t assets_handler(httpd_req_t *req) // generally we dont want other file to see this
{
    ESP_LOGI("ESP_SERVER", "URL:- %s", req->uri); // display the URL
    esp_vfs_spiffs_conf_t esp_vfs_spiffs_conf =
        {
            .base_path = "/spiffs",
            .partition_label = NULL,
            .max_files = 5,
            .format_if_mount_failed = true};
    esp_vfs_spiffs_register(&esp_vfs_spiffs_conf);

    char path[600];
    // routing the file path
    sprintf(path, "/spiffs%s", req->uri); // eg: /spiffs/assets/script.js

    // now link to , .css , .js , extentions
    char *ext = strrchr(path, '.'); // extension :-  path/.js,css,png
    if (strcmp(ext, ".css") == 0)
    {
        httpd_resp_set_type(req, "text/css");
    }
    if (strcmp(ext, ".js") == 0)
    {
        httpd_resp_set_type(req, "text/javascript");
    }

    // handle other files
    FILE *file = fopen(path, "r"); // as string
    if (file == NULL)
    {
        httpd_resp_send_404(req);
        esp_vfs_spiffs_unregister(NULL); // avoid memory leaks
        return ESP_OK;
    }
    char lineRead[256];
    while (fgets(lineRead, sizeof(lineRead), file))
    {
        httpd_resp_sendstr_chunk(req, lineRead);
    }
    httpd_resp_sendstr_chunk(req, NULL);
    fclose(file);
    esp_vfs_spiffs_unregister(NULL); // unmount
    return ESP_OK;
}

esp_err_t img_handler(httpd_req_t *req) // generally we dont want other file to see this
{
    ESP_LOGI("ESP_SERVER", "URL:- %s", req->uri); // display the URL
    esp_vfs_spiffs_conf_t esp_vfs_spiffs_conf =
        {
            .base_path = "/spiffs",
            .partition_label = NULL,
            .max_files = 5,
            .format_if_mount_failed = true};
    esp_vfs_spiffs_register(&esp_vfs_spiffs_conf);

    char path[600];
    // routing the file path
    sprintf(path, "/spiffs%s", req->uri); // eg: /spiffs/assets/script.js

    // now link to , .css , .js , extentions
    char *ext = strrchr(path, '.'); // extension :-  path/.js,css,png

    // for image extensions
    if (strcmp(ext, ".jpg") == 0)
    {
        httpd_resp_set_type(req, "image/jpeg");
    }
    // for image extensions
    if (strcmp(ext, ".jpeg") == 0)
    {
        httpd_resp_set_type(req, "image/jpeg");
    }
    // for image extensions
    if (strcmp(ext, ".png") == 0)
    {
        httpd_resp_set_type(req, "image/png");
    }

    // handle other files
    FILE *file = fopen(path, "rb"); // binary read
    if (file == NULL)
    {
        httpd_resp_send_404(req);
        esp_vfs_spiffs_unregister(NULL); // avoid memory leaks
        return ESP_OK;
    }
    char lineRead[256];
    while (fread(lineRead, sizeof(lineRead), 1, file))
    {
        httpd_resp_send_chunk(req, lineRead, sizeof(lineRead));
    }
    fclose(file);
    esp_vfs_spiffs_unregister(NULL); // unmount
    return ESP_OK;
}

/****************** to scan URLS when [ esp => AP type ]*****************************/
/*static esp_err_t scan_ap_url(httpd_req_t *req)
{
    esp_wifi_set_mode(WIFI_MODE_APSTA);
    wifi_scan_config_t wifi_scan_config = {
        .bssid = NULL,
        .ssid = NULL,
        .channel = 0,
        .show_hidden = false};

    esp_wifi_scan_start(&wifi_scan_config, true);

    wifi_ap_record_t wifi_ap_record[AMX_APs];
    uint16_t ap_count = AMX_APs;
    esp_wifi_scan_get_ap_records(&ap_count, wifi_ap_record);
    cJSON *wifi_scan_json = cJSON_CreateArray(); // creating a json array
    for (size_t i = 0; i < ap_count; i++)
    {
        cJSON *element = cJSON_CreateObject();
        cJSON_AddStringToObject(element, "ssid", (char *)wifi_ap_record[i].ssid); // adding ssid to json obj
        cJSON_AddNumberToObject(element, "rssi", wifi_ap_record[i].rssi);         // adding ssid to json obj
        cJSON_AddItemToArray(wifi_scan_json, element);                            // 1 element added to obj
    }
    char *json_str = cJSON_Print(wifi_scan_json); // store the json_array to pointer "json_str"

    httpd_resp_set_type(req, "application/json"); // set the json type ; for request
    httpd_resp_sendstr(req, json_str);            // sending all the AP records

    // Now write to .json file inside spiffs

    cJSON_Delete(wifi_scan_json);
    free(json_str);
    return ESP_OK;
}
*/
/******************** wifi connesction function: [ESP => AP TO STA] *******************/
void connect_to_local_AP(void *params)
{
    // vTaskDelay(100 / portTICK_PERIOD_MS);
    ap_config_t *ap_config = (ap_config_t *)params; // type casting to ap_config_t

    // wifi_destroy_netif();                               // destroy netif infos [like ip,mac..] for wifi_AP_mode
    wifi_disconnect();     // disconnect the wifi_AP_mode
    dns_hijack_srv_stop(); // just making sure to close
    ESP_LOGE("PARSE_SSID", "%s", ap_config->local_ssid);
    ESP_LOGE("PARSE_PASS", "%s", ap_config->local_pass);

    if (wifi_connect_sta(ap_config->local_ssid, ap_config->local_pass, 15000) == ESP_OK) // if the local ssid/pass doesn't match
    {
        ESP_LOGI("AP_to_STA_TAG", "CONNECTED TO New_SSID... > Saving cred of %s in NVS... ", ap_config->local_ssid);

        // nvs_flash_init(); // no harm in re_initing the nvs
        nvs_handle nvs_write_handle;
        ESP_ERROR_CHECK(nvs_open("wifiCreds", NVS_READWRITE, &nvs_write_handle));            // namespace => local_APs
        ESP_ERROR_CHECK(nvs_set_str(nvs_write_handle, "store_ssid", ap_config->local_ssid)); // key = store_ssid ; value = structure{}
        ESP_ERROR_CHECK(nvs_commit(nvs_write_handle));
        ESP_ERROR_CHECK(nvs_set_str(nvs_write_handle, "store_pass", ap_config->local_pass)); // key = store_pass ; value = structure{}
        ESP_ERROR_CHECK(nvs_commit(nvs_write_handle));
        nvs_close(nvs_write_handle);
        // start http server for login after [ ESP => STA ]
        http_server_sta_mode();
    }
    else
    {
        ESP_LOGE("AP_to_STA_TAG", "Failed to connect to LOCAL_SSID...> Retry!! "); // revert back to AP
        wifi_disconnect();
        wifi_connect_ap("ESP-32_local");
        http_server_ap_mode();
    }
    vTaskDelete(NULL);
}
/******************** handler to activate : AP TO STA connection *******************/
esp_err_t AP_TO_STA(httpd_req_t *req)
{
    ESP_LOGI("ESP_SERVER", "URL:- %s", req->uri); // display the URL
    static ap_config_t ap_config;                 // required to stay in memory

    char buffer[100];
    memset(&buffer, 0, sizeof(buffer));
    httpd_req_recv(req, buffer, req->content_len); // "local_ssid=xxx\r\nlocal_pass=xxxx\r\n"// revieve the contents inside buf
    ESP_LOGW("portal_data", "Buffer : %s", buffer);

    // manual parsing
    // char *ssid = strtok(buffer,"\r\n");
    // char *pass = strtok(NULL,"\r\n");       // NULL -> use the same string "buffer"
    // //ssid=xxx\0
    // //pass=xxx\0
    // ssid = strchr(ssid,'=')+1;  // moving the pointer to right of '='
    // pass = strchr(pass,'=')+1;
    // printf("ssid: %s pass: %s",ssid,pass);
    // nvs_flash_init();
    // nvs_handle nvs_write_handle;
    //     ESP_ERROR_CHECK(nvs_open( "NVS_cred", NVS_READWRITE, &nvs_write_handle));      // namespace => local_APs
    //     ESP_ERROR_CHECK(nvs_set_str(nvs_write_handle,"store_ssid", ap_config->local_ssid));       // key = store_ssid ; value = structure{}
    //     ESP_ERROR_CHECK(nvs_set_str(nvs_write_handle,"store_pass", ap_config->local_pass));       // key = store_pass ; value = structure{}
    //     ESP_ERROR_CHECK(nvs_commit(nvs_write_handle));
    //     nvs_close(nvs_write_handle);

    // alternative parsing
    if ((strstr(buffer, "local_ssid") != NULL) && (strstr(buffer, "local_pass") != NULL))
    {
        cJSON *payload = cJSON_Parse(buffer);
        strcpy(ap_config.local_ssid, cJSON_GetObjectItem(payload, "local_ssid")->valuestring); // this "ap_config" structure is passed to connect to new ssid
        strcpy(ap_config.local_pass, cJSON_GetObjectItem(payload, "local_pass")->valuestring);
        cJSON_Delete(payload);

        httpd_resp_set_status(req, "204 NO CONTENT");
        httpd_resp_send(req, NULL, 0);
        xTaskCreate(connect_to_local_AP, "connect_to_local_ap", 4096, &ap_config, 1, NULL);
    }

    return ESP_OK;
}
/***********************************************/

void Connect_Portal()
{ // only invoke this config portion if server is reset

    if (NULL == server)
    {
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        config.uri_match_fn = httpd_uri_match_wildcard;
        ESP_ERROR_CHECK(httpd_start(&server, &config));
        ESP_LOGW("Server_TAG", "Server Restart Successful...");

        // X. assets [jpg/png]
        httpd_uri_t img_url = {
            .uri = "/image*", // URL added to wifi's default gateway
            .method = HTTP_GET,
            .handler = img_handler, // method thats gonna be hit if url visited
        };
        httpd_register_uri_handler(server, &img_url); // register the uri/url handler/

        // X. assets [js/css]
        httpd_uri_t assets_url = {
            .uri = "/assets*", // URL added to wifi's default gateway
            .method = HTTP_GET,
            .handler = assets_handler, // method thats gonna be hit if url visited
        };
        httpd_register_uri_handler(server, &assets_url); // register the uri/url handler/
    }
}
void http_server_ap_mode(void)
{
    httpd_unregister_uri_handler(server, "/", HTTP_GET);
    httpd_unregister_uri_handler(server, "/dashboard", HTTP_GET);
    httpd_unregister_uri_handler(server, "/login_auth_post", HTTP_POST);

    httpd_uri_t get_ap_list_url = {
        .uri = "/config_portal",
        .method = HTTP_GET,
        .handler = connect_ssid};
    httpd_register_uri_handler(server, &get_ap_list_url);

    httpd_uri_t ap_to_sta_url = {
        .uri = "/AP_STA_post",
        .method = HTTP_POST,
        .handler = AP_TO_STA,
    };
    httpd_register_uri_handler(server, &ap_to_sta_url);

    httpd_uri_t captive_url = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = captive_handler};
    httpd_register_uri_handler(server, &captive_url);
}
void http_server_sta_mode(void)
{
    httpd_unregister_uri_handler(server, "/*", HTTP_GET);
    httpd_unregister_uri_handler(server, "/config_portal", HTTP_GET);
    httpd_unregister_uri_handler(server, "/AP_STA_post", HTTP_POST);
    httpd_uri_t get_ap_list_url = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = login_handler};
    httpd_register_uri_handler(server, &get_ap_list_url);

    httpd_uri_t auth_post = {
        .uri = "/login_auth_post",
        .method = HTTP_POST,
        .handler = login_auth_handler};
    httpd_register_uri_handler(server, &auth_post);

    httpd_uri_t dashboard_url = {
        .uri = "/dashboard",
        .method = HTTP_GET,
        .handler = (dashboard_inUse) ? login_handler : dashboard_handler};
    httpd_register_uri_handler(server, &dashboard_url);
}

void start_dns_server(void)
{
    ip4_addr_t resolve_ip;
    inet_pton(AF_INET, "192.168.1.1", &resolve_ip);
    if (dns_hijack_srv_start(resolve_ip) == ESP_OK)
    {
        ESP_LOGE("DNS_TAG", "DNS server started...");
    }
}

// void start_mdns_service(void)
// {
//     // mdns_ip_addr_t address_list;
//     ESP_ERROR_CHECK(mdns_init());
//     ESP_ERROR_CHECK(mdns_hostname_set(local_server_name));
//     ESP_ERROR_CHECK(mdns_instance_name_set("local_Server_instance"));
//     // esp_netif_str_to_ip4("192.168.1.100", &address_list.addr.u_addr.ip4);
//     // ESP_ERROR_CHECK(mdns_delegate_hostname_add("NDS", &address_list));
//     mdns_txt_item_t serviceTxtData[3] = {
//         {"board", "{esp32}"},
//         {"u", "user"},
//         {"p", "password"}};
//     ESP_ERROR_CHECK(mdns_service_add("ESP32-WebSocket-Server", "_wss", "_tcp", 443, serviceTxtData, 3));
//     ESP_ERROR_CHECK(mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0));
//     ESP_LOGE("MDNS_TAG", "Server_name [STA-Mode] => '%s.local'", local_server_name);
// }
// void resolve_mdns_host(const char *host_name)
// {
//     ESP_LOGE("Resolved_TAG", "Query A: %s.local", host_name);
//     // esp_ip4_addr_t addr = NULL;
//     struct esp_ip4_addr addr;
//     addr.addr = 0;
//     esp_err_t err = mdns_query_a(host_name, 2000, &addr);
//     if (err)
//     {
//         if (err == ESP_ERR_NOT_FOUND)
//         {
//             ESP_LOGE("Resolved_TAG", "Host was not found!");
//             return;
//         }
//         ESP_LOGE("Resolved_TAG", "Query Failed: %s", esp_err_to_name(err));
//         return;
//     }
//     ESP_LOGE("Resolved_TAG", "Query A: %s.local resolved to: " IPSTR, host_name, IP2STR(&addr));
// }
// void stop_mdns_service(void)
// {
//     mdns_free();
//     ESP_LOGE("MDNS_TAG", "Stopping => '%s.local'", local_server_name);
// }