#include <stdio.h>
#include "DATA_FIELD.h"
#include <string.h>
#include <stdlib.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_spiffs.h"
#include "esp_system.h"
#include "toggleled.h"
#include "cJSON.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "connect.h"
#include "dns_hijack_srv.h"
#include "mdns.h"

#define init_mode 0      // nvs storage is empty
#define operation_mode 1 // nvs storage has username & password stored

extern xSemaphoreHandle xSema;
extern uint8_t Relay_Status_Value[RELAY_UPDATE_MAX];   // 1-18
extern uint8_t Relay_Update_Success[RELAY_UPDATE_MAX]; // 1-18
// #define AMX_APs 10 // scans max = 10 AP
// const char *local_server_name = "esp";
httpd_handle_t server = NULL; // for server.c only
static bool dashboard_inUse = false;
static bool relay_inUse = false;

/*countdown variable*/
// #define Unlock_duration 180 // sec
// uint8_t count_sec = Unlock_duration;
// StaticTimer_t static_timer;
// static TimerHandle_t xGTimer = NULL;
// static BaseType_t timer_status = pdFAIL;

/*extern functions*/
extern esp_err_t wifi_connect_sta(const char *SSID, const char *PASS, int timeout);
extern void wifi_connect_ap(const char *SSID);
extern esp_err_t login_cred(auth_t *Data);

void http_server_ap_mode(void);
void http_server_sta_mode(void);

// void CountDown_timer(TimerHandle_t xGTimer) // service routine for rtos-timer
// {
//     ESP_LOGW("Login_Timeout", "\t.....Time left = %d sec", count_sec);
//     if (count_sec > 0)
//     {
//         count_sec--;
//     }
//     else
//     {
//         count_sec = Unlock_duration; // refresh the minute counter
//         response.approve = false;
//         dashboard_inUse = false;
//         timer_status = pdFAIL;
//         xTimerStop(xGTimer, 0);
//     }
// }

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
/*get*/
esp_err_t settings_handler(httpd_req_t *req) // generally we dont want other file to see this
{
    ESP_LOGI("ESP_SERVER", "URL:- %s", req->uri);
    ESP_LOGW("Login_Timeout", "Login_status : %s ....", (response.approve) ? "Unlocked" : "Locked");
    if (response.approve) // && (settings_inUse == false)) // approve = 1 -> Login_timeout = False
    {
        file_open("/spiffs/settings.html", req);
        httpd_resp_send(req, NULL, 0);
    }
    else
    {
        httpd_resp_set_status(req, "302 FOUND");
        httpd_resp_set_hdr(req, "location", "/");
        file_open("/spiffs/nds.html", req);
        httpd_resp_send(req, NULL, 0);
    }
    return ESP_OK;
}
esp_err_t info_handler(httpd_req_t *req) // generally we dont want other file to see this
{
    ESP_LOGI("ESP_SERVER", "URL:- %s", req->uri);
    ESP_LOGW("Login_Timeout", "Login_status : %s ....", (response.approve) ? "Unlocked" : "Locked");
    if (response.approve) // && (info_inUse == false)) // approve = 1 -> Login_timeout = False
    {
        file_open("/spiffs/info.html", req);
        httpd_resp_send(req, NULL, 0);
    }
    else
    {
        httpd_resp_set_status(req, "302 FOUND");
        httpd_resp_set_hdr(req, "location", "/");
        file_open("/spiffs/nds.html", req);
        httpd_resp_send(req, NULL, 0);
    }
    return ESP_OK;
}
// esp_err_t refresh_relay_handler(httpd_req_t *req) // generally we dont want other file to see this
// {
//     ESP_LOGI("ESP_SERVER", "URL:- %s", req->uri);
//     //relay_inUse = false;
//     ESP_LOGE("RELAY_STATUS", "%s", (relay_inUse) ? "InUse" : "Not InUse");
//     file_open("/spiffs/refresh_relay.html", req);
//     httpd_resp_send(req, NULL, 0);
//     return ESP_OK;
// }
esp_err_t relay_handler(httpd_req_t *req) // generally we dont want other file to see this
{
    ESP_LOGI("ESP_SERVER", "URL:- %s", req->uri); // display the URL
    ESP_LOGW("Login_Timeout", "Login_status : %s ....", (response.approve) ? "Unlocked" : "Locked");

    ESP_LOGE("RELAY_STATUS", "%s", (relay_inUse) ? "InUse" : "Not InUse");
    if (response.approve) // && (relay_inUse == false)) // approve = 1 -> Login_timeout = False
    {
        // relay_inUse = true;
        httpd_resp_set_type(req, "text/html");
        file_open("/spiffs/relay.html", req); // Also, can send other response values from here

        // if (Relay_Status_Value[SERIAL_UPDATE] == 0 && Relay_Status_Value[RANDOM_UPDATE] == 0) // for default operation, send back 'relay_status_values' also.
        // {
        //     // generate a json packet of 'Prev_relay_status'.
        //     cJSON *JSON_data = cJSON_CreateObject();
        //     for (uint8_t i = 1; i <= RELAY_UPDATE_16; i++) // get "1/0" -> relay_status [1-16]
        //     {
        //         char *str = (char *)malloc(sizeof("Relay") + 2);
        //         memset(str, 0, sizeof("Relay") + 2);
        //         sprintf(str, "Relay%u", i);
        //         cJSON_AddNumberToObject(JSON_data, str, (Relay_Status_Value[i]) ? 0 : 1); // sending back the inverse to web browser (avoid confusion) ; r_ON => 1 ; r_OFF = 0
        //         free(str);
        //     }
        //     char *string_json = cJSON_Print(JSON_data);
        //     ESP_LOGE("JSON_REPLY", "%s", string_json);
        //     httpd_resp_set_type(req, "application/json"); // sending json data as response
        //     httpd_resp_sendstr(req, string_json);         // chunk
        //     httpd_resp_send(req, NULL, 0);
        //     free(string_json);
        //     cJSON_free(JSON_data);
        //     return ESP_OK;
        // }
    }
    else
    {
        // httpd_resp_set_status(req, "204 NO CONTENT");
        // httpd_resp_send(req, NULL, 0);
        httpd_resp_set_status(req, "302 FOUND");
        httpd_resp_set_hdr(req, "location", "/");
        file_open("/spiffs/nds.html", req);
    }
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}
// esp_err_t refresh_dashboard_handler(httpd_req_t *req) // generally we dont want other file to see this
// {
//     ESP_LOGI("ESP_SERVER", "URL:- %s", req->uri);
//     dashboard_inUse = false;
//     ESP_LOGE("DASHBOARD_STATUS", "%s", (dashboard_inUse) ? "InUse" : "Not InUse");
//     file_open("/spiffs/refresh_dashboard.html", req);
//     httpd_resp_send(req, NULL, 0);
//     return ESP_OK;
// }
esp_err_t dashboard_handler(httpd_req_t *req) // generally we dont want other file to see this
{
    ESP_LOGI("ESP_SERVER", "URL:- %s", req->uri); // display the URL
    // ESP_LOGW("Login_Timeout", "Login_status : %s ....", (response.approve) ? "Unlocked" : "Locked");
    // ESP_LOGW("Login_Timeout", "\t\t\t.....Time left = %d sec", count_sec);
    // if (xGTimer == NULL)
    //     xGTimer = xTimerCreateStatic("MY_Countdown_timer", pdMS_TO_TICKS(1000), pdTRUE, NULL, CountDown_timer, &static_timer);
    ESP_LOGE("DASHBOARD_STATUS", "%s", (dashboard_inUse) ? "InUse" : "Not InUse");

    if (response.approve) //&& (dashboard_inUse == false)) // approve = 1 -> Login_timeout = False
    {
        // dashboard_inUse = true;
        //  count_sec = Unlock_duration;
        //  if (timer_status != pdTRUE)
        //   {
        //       timer_status = xTimerStart(xGTimer, 0); // timer_status => pdTrue
        //   }
        file_open("/spiffs/dashboard.html", req); // Also, can send other response values from here
        httpd_resp_send(req, NULL, 0);
    }
    else
    {
        /*generate an alert from "dashboard.js" when reloading in locked status (response => 302)*/
        httpd_resp_set_status(req, "302 FOUND");
        httpd_resp_set_hdr(req, "location", "/");
        file_open("/spiffs/nds.html", req);
        httpd_resp_send(req, NULL, 0);
    }
    ESP_LOGE("DASHBOARD_STATUS", "%s", (dashboard_inUse) ? "InUse" : "Not InUse");
    return ESP_OK;
}
esp_err_t login_handler(httpd_req_t *req) // generally we dont want other file to see this
{
    ESP_LOGI("ESP_SERVER", "URL:- %s", req->uri); // display the URL
    ESP_LOGE("DASHBOARD_STATUS", "%s", (dashboard_inUse) ? "DashBoard currently in use! Please login" : "Free to Use!.");
    ESP_LOGE("RELAY_STATUS", "%s", (relay_inUse) ? "Relay Menu currently in use! Please login" : "Free to Use!.");
    // dashboard_inUse = false;
    // relay_inUse = false;
    response.approve = false;
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

/*post*/
esp_err_t settings_post_handler(httpd_req_t *req) // invoked when login_post is activated
{
    ESP_LOGI("ESP_SERVER", "URL:- %s", req->uri); // display the URL
    static settings_pass_name_t set_pass_name;
    set_t esp;
    char buffer[150];
    memset(&buffer, 0, sizeof(buffer));
    httpd_req_recv(req, buffer, req->content_len); // size_t recv_size = (req->content_len) > sizeof(buffer) ? sizeof(buffer) : (req->content_len);
    ESP_LOGW("settings_post", "Buffer : %s", buffer);

    cJSON *payload = cJSON_Parse(buffer); // returns an object holding respective [ key:value pair data ]
    strcpy(set_pass_name.current_password, cJSON_GetObjectItem(payload, "current_password")->valuestring);
    strcpy(set_pass_name.new_password, cJSON_GetObjectItem(payload, "new_password")->valuestring);
    strcpy(set_pass_name.confirm_password, cJSON_GetObjectItem(payload, "confirm_password")->valuestring);
    strcpy(set_pass_name.new_username, cJSON_GetObjectItem(payload, "new_username")->valuestring);
    cJSON_Delete(payload);

    if (strcmp(set_pass_name.new_password, set_pass_name.confirm_password) == 0)
    {
        esp.send = true;
        // read the internal password and compare with current
        nvs_handle_t set_pass_handle;
        ESP_ERROR_CHECK(nvs_open("loginCreds", NVS_READWRITE, &set_pass_handle));
        size_t required_size;
        nvs_get_str(set_pass_handle, "password", NULL, &required_size); // dynamic allocation and required_size => length required to hold the value.
        char *sample = malloc(required_size);
        switch (nvs_get_str(set_pass_handle, "password", sample, &required_size))
        {
        case ESP_ERR_NVS_NOT_FOUND:
            esp.receive = false;
            ESP_LOGE("Setting_New_User_Pass_TAG", "Prev-password [To-be replaced] => not found...., Retry with Default pass :- ADMIN");
            ESP_ERROR_CHECK(nvs_set_str(set_pass_handle, "password", "ADMIN"));
            ESP_ERROR_CHECK(nvs_commit(set_pass_handle));
            break;
        case ESP_OK:
            ESP_LOGW("Setting_New_User_Pass_TAG", "NVS_stored : Prev-password is => %s [To-be replaced] by New-password =>%s ", sample, set_pass_name.confirm_password);
            (strcmp(set_pass_name.current_password, sample) == 0) ? (esp.receive = true) : (esp.receive = false); // true => match
            // Now setup new password, if all conditions matches
            break;
        default:
            // forgot pass
            break;
        }
        if (esp.receive)
        {
            ESP_LOGI("Setting_New_User_Pass_TAG", "Storing New-pass : %s", set_pass_name.confirm_password);
            ESP_ERROR_CHECK(nvs_set_str(set_pass_handle, "password", set_pass_name.confirm_password));
            ESP_ERROR_CHECK(nvs_commit(set_pass_handle));
        }
        // else
        //     ESP_ERROR_CHECK(nvs_set_str(set_pass_handle, "password", set_pass.confirm_password));

        if (sample != NULL)
            free(sample);
        nvs_close(set_pass_handle);
    }
    else
    {
        esp.send = false;
    }

    // Preparing json data
    cJSON *JSON_data = cJSON_CreateObject();
    (esp.send && esp.receive) ? (cJSON_AddNumberToObject(JSON_data, "password_set_success", 1)) : (cJSON_AddNumberToObject(JSON_data, "password_set_success", 0));
    char *string_json = cJSON_Print(JSON_data);
    ESP_LOGE("JSON_REPLY", "%s", string_json);
    httpd_resp_set_type(req, "application/json"); // sending json data as response
    httpd_resp_sendstr(req, string_json);         // chunk
    httpd_resp_send(req, NULL, 0);
    free(string_json);
    cJSON_free(JSON_data);

    return ESP_OK;
}

esp_err_t info_post_handler(httpd_req_t *req) // invoked when login_post is activated
{
    ESP_LOGI("ESP_SERVER", "URL:- %s", req->uri); // display the URL
    set_t esp;
    char buffer[5];
    memset(&buffer, 0, sizeof(buffer));
    httpd_req_recv(req, buffer, req->content_len); // size_t recv_size = (req->content_len) > sizeof(buffer) ? sizeof(buffer) : (req->content_len);
    ESP_LOGW("Info_post", "Buffer : %s", buffer);

    cJSON *payload = cJSON_Parse(buffer); // returns an object holding respective [ key:value pair data ]
    esp.receive = cJSON_GetObjectItem(payload, "InfoReq")->valueint;
    ESP_LOGI("Parse_tag", "Info Requested => %s", (esp.receive) ? "True" : "False");
    cJSON_Delete(payload);
    if (esp.receive)
    {
        // Preparing json data
        uint16_t val = 0;
        uint8_t chipId[6];
        int sysUpTime[4];
        char timestamp[30];
        struct tm Start_Time;
        esp_chip_info_t chip_info;
        const char *compile_date2 = __DATE__;
        const char *compile_time2 = __TIME__;
        memset(&chipId, 0, sizeof(chipId));
        memset(&sysUpTime, 0, sizeof(sysUpTime));
        char *MAC = (char *)malloc(sizeof(chipId) * 6);
        char *Uptime = (char *)malloc(15 + (sizeof(int) * 4));

        /*resource calculation*/
        esp_chip_info(&chip_info);
        int FreeHeap = esp_get_free_heap_size();
        int DRam = heap_caps_get_free_size(MALLOC_CAP_8BIT);
        int IRam = heap_caps_get_free_size(MALLOC_CAP_32BIT) - heap_caps_get_free_size(MALLOC_CAP_8BIT);
        int LargestFreeHeap = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
        esp_efuse_mac_get_default(chipId);
        sprintf(MAC, "%02x:%02x:%02x:%02x:%02x:%02x", chipId[0], chipId[1], chipId[2], chipId[3], chipId[4], chipId[5]);

        /*Compile time*/
        strptime(compile_date2, "%b %d %Y", &Start_Time);
        strptime(compile_time2, "%I:%M:%S", &Start_Time);
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %I:%M:%S.%p", &Start_Time); // convert date and time to string

        /*Up time*/
        unsigned long millis = esp_timer_get_time() / 1000;
        sysUpTime[0] = (int)(millis / (1000)) % 60;
        sysUpTime[1] = (int)(millis / (1000 * 60)) % 60;
        sysUpTime[2] = (int)(millis / (1000 * 60 * 60)) % 24;
        sysUpTime[3] = (int)(millis / (1000 * 60 * 60 * 24)) % 365;
        sprintf(Uptime, "%d Day:%dHr:%dMin:%dSec", sysUpTime[3], sysUpTime[2], sysUpTime[1], sysUpTime[0]);

        /*boot_count*/
        nvs_handle BOOT_info;
        ESP_ERROR_CHECK(nvs_open("bootVal", NVS_READWRITE, &BOOT_info));
        ESP_ERROR_CHECK(nvs_get_u16(BOOT_info, "COUNT", &val));
        ESP_ERROR_CHECK(nvs_commit(BOOT_info));
        nvs_close(BOOT_info);

        /*create json packet*/
        cJSON *JSON_data = cJSON_CreateObject();
        cJSON_AddNumberToObject(JSON_data, "Approve", 1);
        cJSON_AddStringToObject(JSON_data, "CHIP_MODEL", "ESP_32");
        cJSON_AddStringToObject(JSON_data, "CHIP_ID_MAC", MAC);
        cJSON_AddNumberToObject(JSON_data, "CHIP_CORES", chip_info.cores);
        cJSON_AddNumberToObject(JSON_data, "FLASH_SIZE", (spi_flash_get_chip_size() / (1024 * 1024)));
        cJSON_AddNumberToObject(JSON_data, "HEAP_SIZE", FreeHeap);
        cJSON_AddNumberToObject(JSON_data, "FREE_DRAM", DRam / 1024);
        cJSON_AddNumberToObject(JSON_data, "FREE_IRAM", IRam / 1024);
        cJSON_AddNumberToObject(JSON_data, "FREE_HEAP", LargestFreeHeap);
        cJSON_AddNumberToObject(JSON_data, "BOOT_COUNT", val);
        cJSON_AddStringToObject(JSON_data, "COMPILE_TIME", timestamp);
        cJSON_AddStringToObject(JSON_data, "UP_TIME", Uptime);
        // sending the json packet
        char *string_json = cJSON_Print(JSON_data);
        ESP_LOGE("JSON_REPLY", "%s", string_json);
        httpd_resp_set_type(req, "application/json"); // sending json data as response
        httpd_resp_sendstr(req, string_json);         // chunk
        httpd_resp_send(req, NULL, 0);

        free(string_json);
        cJSON_free(JSON_data);
        free(MAC);
        free(Uptime);
    }
    return ESP_OK;
}

esp_err_t relay_json_post_handler(httpd_req_t *req) // invoked when login_post is activated
{
    ESP_LOGI("ESP_SERVER", "URL:- %s", req->uri);
    char buffer[250];
    memset(&buffer, 0, sizeof(buffer));
    httpd_req_recv(req, buffer, req->content_len); // size_t recv_size = (req->content_len) > sizeof(buffer) ? sizeof(buffer) : (req->content_len);
    ESP_LOGW("RELAY_STATE_POST", "Buffer : %s", buffer);
    /********************************************* PARSING INCOMING JSON *********************************************************************************/
    // Parsing json structured credentials
    if (strstr(buffer, ":") != NULL)
    {
        // parsing json data into Relay_status_array
        cJSON *payload = cJSON_Parse(buffer); // returns an object holding respective [ key:value pair data ]
        Relay_Status_Value[SERIAL_UPDATE] = (cJSON_GetObjectItem(payload, "serial")->valueint);
        Relay_Status_Value[RANDOM_UPDATE] = (cJSON_GetObjectItem(payload, "random")->valueint);
        for (uint8_t i = 1; i <= 16; i++)
        {
            char *str = (char *)malloc(sizeof("Relay") + 2);
            memset(str, 0, sizeof("Relay") + 2);
            sprintf(str, "Relay%u", i);
            Relay_Status_Value[i] = (cJSON_GetObjectItem(payload, str)->valueint) ? 0 : 1; // invert logic for relay [i.e :- {ON = 0} & {OFF = 1}]
            free(str);
        }
        cJSON_Delete(payload);
        ESP_LOGE("RELAY_JSON_POST_PARSE", "serial = %d , random = %d", Relay_Status_Value[SERIAL_UPDATE], Relay_Status_Value[RANDOM_UPDATE]);
        /******************************************* STORING NEW RELAY STATUS ******************************************************************/
        // Storing button status in nvs-storage
        nvs_handle_t nvs_relay;
        nvs_open("Relay_Status", NVS_READWRITE, &nvs_relay);                                  // namespace => Relay_Status [creates ]
        if (Relay_Status_Value[SERIAL_UPDATE] == 0 && Relay_Status_Value[RANDOM_UPDATE] == 0) // First, storing the button status [relay1 - to - relay16]
        {
            for (uint8_t i = 1; i <= 16; i++) // Default state
            {
                char *str = (char *)malloc(sizeof("Relay") + 2);
                memset(str, 0, sizeof("Relay") + 2);
                sprintf(str, "Relay%u", i);
                nvs_set_u8(nvs_relay, str, Relay_Status_Value[i]);
                nvs_commit(nvs_relay);
                free(str);
            }
        }
        else // clear all stored relay state for special operations
        {
            for (uint8_t i = 1; i <= 16; i++) // Special state...., i.e. [If: S/R != 0] , Store 'r_OFF' for all relay status, in internal nvs storage
            {
                char *str = (char *)malloc(sizeof("Relay") + 2);
                memset(str, 0, sizeof("Relay") + 2);
                sprintf(str, "Relay%u", i);
                nvs_set_u8(nvs_relay, str, (uint8_t)1); // 1 -> r_OFF
                nvs_commit(nvs_relay);
                free(str);
            }
        }
        nvs_set_u8(nvs_relay, "random", Relay_Status_Value[RANDOM_UPDATE]); // random => [0/0ff] vs [1/ON , 2/ON , 3/ON , 4/ON]
        nvs_commit(nvs_relay);
        nvs_set_u8(nvs_relay, "serial", Relay_Status_Value[SERIAL_UPDATE]); // serial => [0/0ff] vs [1/ON]
        nvs_commit(nvs_relay);
        nvs_close(nvs_relay);
        vTaskDelay(pdMS_TO_TICKS(5));

        /******************************************** TURN ON/OFF GPIO_PINS *****************************************************************/
        xSemaphoreGive(xSema); // retrieve the stored data, Invoke the relay switches and generate "update_success_status" values
        vTaskDelay(250 / portTICK_PERIOD_MS);
        /********************************************* CREATING JSON ****************************************************************/
        // creating new json packet to send the success_status as reply
        cJSON *JSON_data = cJSON_CreateObject();
        cJSON_AddNumberToObject(JSON_data, "random_update_success", Relay_Update_Success[RANDOM_UPDATE]);
        cJSON_AddNumberToObject(JSON_data, "serial_update_success", Relay_Update_Success[SERIAL_UPDATE]);
        for (uint8_t i = 1; i <= RELAY_UPDATE_16; i++) // get "1/0" -> relay_status [1-16]
        {
            char *str = (char *)malloc(sizeof("Relay_update_success") + 2);
            memset(str, 0, sizeof("Relay_update_success") + 2);
            sprintf(str, "Relay%u_update_success", i);
            cJSON_AddNumberToObject(JSON_data, str, Relay_Update_Success[i]);
            free(str);
        }
        if (Relay_Status_Value[SERIAL_UPDATE] == 0 && Relay_Status_Value[RANDOM_UPDATE] == 0) // for default operation, send back 'relay_status_values' also.
        {
            for (uint8_t i = 1; i <= RELAY_UPDATE_16; i++) // get "1/0" -> relay_status [1-16]
            {
                char *str = (char *)malloc(sizeof("Relay") + 2);
                memset(str, 0, sizeof("Relay") + 2);
                sprintf(str, "Relay%u", i);
                cJSON_AddNumberToObject(JSON_data, str, (Relay_Status_Value[i]) ? 0 : 1); // sending back the inverse to web browser (avoid confusion) ; r_ON => 1 ; r_OFF = 0
                free(str);
            }
        }
        char *string_json = cJSON_Print(JSON_data);
        ESP_LOGE("JSON_REPLY", "%s", string_json);
        /******************************************  SENDING *******************************************************************/
        // sending json reply packet
        httpd_resp_set_type(req, "application/json"); // sending json data as response
        httpd_resp_sendstr(req, string_json);         // chunk
        httpd_resp_send(req, NULL, 0);
        free(string_json);
        cJSON_free(JSON_data);
    }
    /***********************************************************************************************************************************************/
    else
    {
        ESP_LOGE("RELAY_INPUT", "Incomming JSON_packet not proper!!");
        httpd_resp_set_status(req, "204 NO CONTENT");
        httpd_resp_send(req, NULL, 0);
    }
    return ESP_OK;
}

esp_err_t restart_handler(httpd_req_t *req) // invoked when login_post is activated
{
    ESP_LOGI("ESP_SERVER", "URL:- %s", req->uri); // display the URL
    set_t esp;
    char buffer[5]; // this stores username and password into buffer //
    memset(&buffer, 0, sizeof(buffer));
    httpd_req_recv(req, buffer, req->content_len); // size_t recv_size = (req->content_len) > sizeof(buffer) ? sizeof(buffer) : (req->content_len);
    ESP_LOGW("restart_post", "Buffer : %s", buffer);

    cJSON *payload = cJSON_Parse(buffer); // returns an object holding respective [ key:value pair data ]
    esp.receive = cJSON_GetObjectItem(payload, "restart")->valueint;
    ESP_LOGI("Parse_tag", "Restart Requested => %s", (esp.receive) ? "True" : "False");
    cJSON_Delete(payload);

    // Preparing json data
    cJSON *JSON_data = cJSON_CreateObject();
    cJSON_AddNumberToObject(JSON_data, "approve", 1); // approve -> 'restart'
    char *string_json = cJSON_Print(JSON_data);
    ESP_LOGE("JSON_REPLY", "%s", string_json);
    httpd_resp_set_type(req, "application/json"); // sending json data as response
    httpd_resp_sendstr(req, string_json);         // chunk
    httpd_resp_send(req, NULL, 0);

    free(string_json);
    cJSON_free(JSON_data);

    if (esp.receive)
    {
        for (int i = 3; i > 0; i--)
        {
            ESP_LOGE("RESTART_TAG", "Restarting in ... %d", i);
            vTaskDelay(750 / portTICK_PERIOD_MS);
        }
        ESP_LOGE("RESTARTING ESP_32", "NOW...");
        vTaskDelay(pdMS_TO_TICKS(10));
        esp_restart();
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
    cJSON *payload = cJSON_Parse(buffer); // returns an object holding respective [ key:value pair data ]
    strcpy(cred.username, cJSON_GetObjectItem(payload, "username")->valuestring);
    strcpy(cred.password, cJSON_GetObjectItem(payload, "password")->valuestring);
    ESP_LOGI("Parse_tag", "USERNAME = %s", cred.username);
    ESP_LOGI("Parse_tag", "PASSWORD = %s", cred.password);
    cJSON_Delete(payload);

    //     manual parsing
    //     char *U = strtok(buffer, "\r\n");
    //     char *P = strtok(NULL, "\r\n");
    //     U = strchr(U, '=') + 1;          // RIKESH
    //     P = strchr(P, '=') + 1;          // PASS
    //     sprintf(cred.username, "%s", U); //
    //     sprintf(cred.password, "%s", P);
    //     ESP_LOGE("Parse_tag", "%s", cred.username);
    //     ESP_LOGE("Parse_tag", "%s", cred.password);

    // first decide the login_mode // response.approve = true []
    response.login_mode = (login_cred(&cred) == ESP_FAIL) ? (init_mode) : (operation_mode); // [response.login_mode = 0; init_mode] // [response.login_mode = 1; operation_mode]

    // Then compare the username and password
    if (response.login_mode == init_mode) // init_mode -> 0
    {
        ESP_LOGI("AUTH_TAG", "....Init_Mode....");
        if ((strcmp(cred.username, "ADMIN") == 0) && (strcmp(cred.password, "ADMIN") == 0))
        {
            response.approve = true;
            ESP_LOGW("AUTH_TAG", "USERNAME & PASSWORD........ Approved");
        }
        else
        {
            response.approve = false;
            ESP_LOGE("AUTH_TAG", "Please reload into same IP. Incorrect username/password");
            httpd_resp_set_status(req, "204 NO CONTENT");
            httpd_resp_send(req, NULL, 0);
            return ESP_OK;
        }
    }
    else if (response.login_mode == operation_mode) // If entered & stored: username and password match, Then login_cred will generate --> [response.approve = true]
    {
        ESP_LOGI("AUTH_TAG", "....Operation_Mode....");
        if (response.approve == false)
        {
            ESP_LOGE("AUTH_TAG", "Please reload into same IP. Incorrect username/password");
            httpd_resp_set_status(req, "204 NO CONTENT");
            httpd_resp_send(req, NULL, 0);
            return ESP_OK;
        }
    }

    // sending json data
    cJSON *JSON_data = cJSON_CreateObject();
    cJSON_AddNumberToObject(JSON_data, "approve", (uint8_t)response.approve);
    cJSON_AddNumberToObject(JSON_data, "login_mode", (uint8_t)response.login_mode);
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

/******************** local_wifi connection function: [ESP => AP TO STA] *******************/
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
        ESP_LOGE("AP_TO_STA_RESTART", "STA Connection successful");
        for (uint8_t a = 3; a > 0; a--)
        {
            ESP_LOGE("AP_TO_STA_RESTART", "Restarting STA mode in ...%d", a);
            vTaskDelay(pdMS_TO_TICKS(750));
        }
        wifi_disconnect();
        vTaskDelay(pdMS_TO_TICKS(10));
        esp_restart();
    }
    else
    {
        ESP_LOGE("AP_to_STA_TAG", "Failed to connect to LOCAL_SSID...> Retry!! "); // revert back to AP
        wifi_disconnect();
        wifi_connect_ap("ESP-32_local");
    }
    vTaskDelete(NULL);
}
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

        cJSON *JSON_data = cJSON_CreateObject();
        cJSON_AddNumberToObject(JSON_data, "IP_addr3", 0); // INTERNAL STORAGE
        char *string_json = cJSON_Print(JSON_data);
        ESP_LOGE("JSON_REPLY", "%s", string_json);
        httpd_resp_set_type(req, "application/json"); // sending json data as response
        httpd_resp_sendstr(req, string_json);         // chunk
        httpd_resp_send(req, NULL, 0);

        free(string_json);
        cJSON_free(JSON_data);

        vTaskDelay(1000 / portTICK_PERIOD_MS);
        xTaskCreate(connect_to_local_AP, "connect_to_local_ap", 4096, &ap_config, 1, NULL);
    }
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
/*******************************************************************************************/

void Connect_Portal()
{ // only invoke this config portion if server is reset

    if (NULL == server)
    {
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        config.max_uri_handlers = 16;
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
    httpd_unregister_uri_handler(server, "/info", HTTP_GET);
    httpd_unregister_uri_handler(server, "/dashboard", HTTP_GET);
    // httpd_unregister_uri_handler(server, "/refresh_dashboard", HTTP_GET);
    httpd_unregister_uri_handler(server, "/relay", HTTP_GET);
    httpd_unregister_uri_handler(server, "/settings", HTTP_GET);
    // httpd_unregister_uri_handler(server, "/refresh_relay", HTTP_GET);
    httpd_unregister_uri_handler(server, "/info_post", HTTP_POST);
    httpd_unregister_uri_handler(server, "/settings_post", HTTP_POST);
    httpd_unregister_uri_handler(server, "/restart", HTTP_POST);
    httpd_unregister_uri_handler(server, "/login_auth_post", HTTP_POST);
    httpd_unregister_uri_handler(server, "/relay_json_post", HTTP_POST);

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
    httpd_uri_t get_info_url = {
        .uri = "/info",
        .method = HTTP_GET,
        .handler = info_handler};
    //.handler = (dashboard_inUse) ? login_handler : info_handler};
    httpd_register_uri_handler(server, &get_info_url);

    httpd_uri_t dashboard_url = {
        .uri = "/dashboard",
        .method = HTTP_GET,
        .handler = dashboard_handler};
    //.handler = (dashboard_inUse) ? login_handler : dashboard_handler};
    httpd_register_uri_handler(server, &dashboard_url);

    // httpd_uri_t refresh_dashboard_url = {
    //     .uri = "/refresh_dashboard",
    //     .method = HTTP_GET,
    //     .handler = refresh_dashboard_handler};
    // httpd_register_uri_handler(server, &refresh_dashboard_url);

    httpd_uri_t relay_url = {
        .uri = "/relay",
        .method = HTTP_GET,
        .handler = relay_handler};
    // .handler = (relay_inUse) ? login_handler : relay_handler};
    httpd_register_uri_handler(server, &relay_url);
    httpd_uri_t settings_url = {
        .uri = "/settings",
        .method = HTTP_GET,
        .handler = settings_handler};
    // .handler = (dashboard_inUse) ? login_handler : settings_handler};
    httpd_register_uri_handler(server, &settings_url);

    // httpd_uri_t refresh_relay_url = {
    //     .uri = "/refresh_relay",
    //     .method = HTTP_GET,
    //     .handler = refresh_relay_handler};
    // httpd_register_uri_handler(server, &refresh_relay_url);

    httpd_uri_t auth_post = {
        .uri = "/login_auth_post",
        .method = HTTP_POST,
        .handler = login_auth_handler};
    httpd_register_uri_handler(server, &auth_post);

    httpd_uri_t info_post = {
        .uri = "/info_post",
        .method = HTTP_POST,
        .handler = info_post_handler};
    httpd_register_uri_handler(server, &info_post);

    httpd_uri_t restart_post = {
        .uri = "/restart",
        .method = HTTP_POST,
        .handler = restart_handler};
    httpd_register_uri_handler(server, &restart_post);

    httpd_uri_t settings_post = {
        .uri = "/settings_post",
        .method = HTTP_POST,
        .handler = settings_post_handler};
    httpd_register_uri_handler(server, &settings_post);

    httpd_uri_t relay_json_post_url = {
        .uri = "/relay_json_post",
        .method = HTTP_POST,
        .handler = relay_json_post_handler};
    httpd_register_uri_handler(server, &relay_json_post_url);
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