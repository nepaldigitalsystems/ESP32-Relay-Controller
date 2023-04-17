/*******************************************************************************
 *                          Include Files
 *******************************************************************************/
#include <stdio.h>
#include "DATA_FIELD.h"
#include "relay_pattern.h"
#include "connect.h"
#include <string.h>
#include <stdlib.h>
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_spiffs.h"
#include "esp_system.h"
#include "cJSON.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "dns_hijack_srv.h"
#include "netdb.h"
#include "math.h"

/*******************************************************************************
 *                          Static Data Definitions
 *******************************************************************************/
/* Default values of LoginCreds*/
#define default_username "adminuser"
#define default_password "adminpass"
/* login cred index */
#define username_index 0
#define password_index 1

// const char *local_server_name = "nds-esp32";
static httpd_handle_t server = NULL;                         // for server.c only
static uint8_t Relay_inStatus_Value[RELAY_UPDATE_MAX] = {0}; // 1-18

/*extern variables*/
extern uint32_t STA_ADDR3;
extern xSemaphoreHandle xSEMA;
extern uint8_t Relay_Status_Value[RELAY_UPDATE_MAX];
extern uint8_t Relay_Update_Success[RELAY_UPDATE_MAX]; // 1-18

/*extern functions*/
extern esp_err_t wifi_connect_sta(const char *SSID, const char *PASS, int timeout);
extern void wifi_connect_ap(const char *SSID);

/*******************************************************************************
 *                          Static Function Definitions
 *******************************************************************************/
/**
 * @brief function to inspect the user_login_data within NVS_storage.
 *
 * @param Data Array to store the retieved login_cred data from NVS_storage
 * @param login Storage Handle obtained from nvs_open function [to read/write]
 * @param KEY Name of the KEY corresponding to [user_name | user_pass]
 * @param index Index to determine what to select [user_name => 0 | user_pass => 1]
 *
 * @return - ESP_OK: user_name and user_pass exist in NVS_Storage.
 * @return - ESP_FAIL: user_name and user_pass doesn't exist in NVS_Storage or login_creds recieved is not authorized.
 */
static esp_err_t inspect_login_data(auth_t *Data, nvs_handle *login, const char *KEY, uint8_t index)
{
    esp_err_t err = ESP_FAIL; // ESP_FAIL -> NVS_EMPTY -> ADMIN
    size_t required_size;
    nvs_get_str(*login, KEY, NULL, &required_size); // dynamic allocation and required_size => length required to hold the value.
    char *sample = malloc(required_size);

    switch (nvs_get_str(*login, KEY, sample, &required_size)) // extracted the 'username / password' into 'sample' variable
    {
    case ESP_ERR_NVS_NOT_FOUND:
        ESP_LOGE("LOGIN_TAG", "%s not set yet.... Default %s : %s", KEY, KEY, ((index) ? default_password : default_username)); // [data doesn't exist] ---> use :- name & pass => ADMIN & ADMIN
        err = 1;
        ESP_ERROR_CHECK(nvs_set_str(*login, KEY, ((index) ? default_password : default_username))); // storing [index:0->default_username] & [index:1->default_password], if nvs is empty.
        break;
    case ESP_OK:
        err = ESP_OK; // [data exists] ----> correct logins ----> [response.approve = true;
        ESP_LOGW("LOGIN_TAG", "NVS_stored : %s is = %s", KEY, sample);
        if (0 == index)
        {
            if (0 == strcmp(Data->username, sample))
            {
                ESP_LOGW("LOGIN_TAG", "USERNAME ........ Approved");
            }
            else
            {
                ESP_LOGE("LOGIN_TAG", "WRONG USERNAME ENTRY ........ Not Approved");
                err = ESP_FAIL;
            }
        }
        else if (1 == index)
        {
            if (0 == strcmp(Data->password, sample))
            {
                ESP_LOGW("LOGIN_TAG", "PASSWORD ........ Approved");
            }
            else
            {
                ESP_LOGE("LOGIN_TAG", "WRONG PASSWORD ENTRY ........ Not Approved");
                err = ESP_FAIL;
            }
        }
        break;
    }
    ESP_ERROR_CHECK(nvs_commit(*login));
    if (sample != NULL)
        free(sample);
    return err;
}

/**
 * @brief function to inspect the user_login_data within NVS_storage.
 *
 * @param Data array to store the retieved login_cred data from NVS_storage
 *
 * @return - ESP_OK: user_name and user_pass exist in NVS_Storage.
 * @return - ESP_FAIL: user_name and user_pass doesn't exist in NVS_Storage or login_creds recieved is not authorized.
 */
static esp_err_t login_cred(auth_t *Data) // compares the internal login creds with given "Data" arg
{
    nvs_handle login;
    esp_err_t err1 = ESP_FAIL, err2 = ESP_FAIL;
    if (ESP_OK == nvs_open("loginCreds", NVS_READWRITE, &login)) // namespace => loginCreds
    {
        err1 = inspect_login_data(Data, &login, "username", username_index);
        err2 = inspect_login_data(Data, &login, "password", password_index);
        nvs_close(login);
    }
    if ((1 == err1) && (1 == err2))
    {
        ESP_LOGE("Login_cred_change", ".... RETURNS '1'[i.e. no creds; storing default] ....");
        return 1;
    }
    else if ((ESP_OK == err1) && (ESP_OK == err2))
    {
        ESP_LOGE("Login_cred_change", ".... RETURNS 'ESP_0K'[i.e. cred matched] ....");
        return ESP_OK;
    }
    else
    {
        ESP_LOGE("Login_cred_change", ".... RETURNS 'ESP_FAIL'[i.e. cred not matched] ....");
        return ESP_FAIL;
    }
}

/**
 * @brief function to Open a file.
 *
 * @param path Pointer that points to file path
 * @param req Pointer to the request being responded.
 *
 * @return - ESP_OK: Compeleted file opening operation.
 */
static esp_err_t file_open(char path[], httpd_req_t *req)
{
    esp_vfs_spiffs_conf_t esp_vfs_spiffs_conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 10,
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
/*get*/
/**
 * @brief Get Request Handler for settings_page on web.
 * @param req Pointer to the request being responded.
 * @return - ESP_OK: Compeleted operation.
 */
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

/**
 * @brief Get Request Handler for info_page on web.
 * @param req Pointer to the request being responded.
 * @return - ESP_OK: Compeleted operation.
 */
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

/**
 * @brief Get Request Handler for relay_page on web.
 * @param req Pointer to the request being responded.
 * @return - ESP_OK: Compeleted operation.
 */
esp_err_t relay_handler(httpd_req_t *req) // generally we dont want other file to see this
{
    ESP_LOGI("ESP_SERVER", "URL:- %s", req->uri); // display the URL
    ESP_LOGW("Login_Timeout", "Login_status : %s ....", (response.approve) ? "Unlocked" : "Locked");
    if (response.approve) // && (relay_inUse == false)) // approve = 1 -> Login_timeout = False
    {
        httpd_resp_set_type(req, "text/html");
        file_open("/spiffs/relay.html", req); // Also, can send other response values from here
    }
    else
    {
        httpd_resp_set_status(req, "302 FOUND");
        httpd_resp_set_hdr(req, "location", "/");
        file_open("/spiffs/nds.html", req);
    }

    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

/**
 * @brief Get Request Handler for dashboard_page on web.
 * @param req Pointer to the request being responded.
 * @return - ESP_OK: Compeleted operation.
 */
esp_err_t dashboard_handler(httpd_req_t *req) // generally we dont want other file to see this
{
    ESP_LOGI("ESP_SERVER", "URL:- %s", req->uri); // display the URL
    if (response.approve)                         // approve = 1 -> Login_timeout = False
    {
        file_open("/spiffs/dashboard.html", req); // Also, can send other response values from here
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

/**
 * @brief Get Request Handler for login_page on web.
 * @param req Pointer to the request being responded.
 * @return - ESP_OK: Compeleted operation.
 */
esp_err_t login_handler(httpd_req_t *req) // generally we dont want other file to see this
{
    ESP_LOGI("ESP_SERVER", "URL:- %s", req->uri); // display the URL
    response.approve = false;
    return file_open("/spiffs/nds.html", req);
}

/**
 * @brief Get Request Handler to redirect user to captive_portal on web.
 * @param req Pointer to the request being responded.
 * @return - ESP_OK: Compeleted operation.
 */
esp_err_t connect_ssid(httpd_req_t *req) // generally we dont want other file to see this
{
    ESP_LOGI("ESP_SERVER", "URL:- %s", req->uri);
    return file_open("/spiffs/wifi_connect.html", req);
}

/**
 * @brief Get Request Handler for captive_portal_page on web.
 * @param req Pointer to the request being responded.
 * @return - ESP_OK: Compeleted operation.
 */
esp_err_t captive_handler(httpd_req_t *req) // generally we dont want other file to see this
{
    ESP_LOGI("ESP_SERVER", "URL:- %s", req->uri);
    char host[50];
    esp_err_t ret = httpd_req_get_hdr_value_str(req, "Host", host, sizeof(host) - 1);
    ESP_LOGE("HOST_TAG", "Incoming header : %s", host);
    // else if (strcmp(host, "connectivitycheck.gstatic.com") == 0)
    // else if (strcmp(host, "connectivitycheck.android.com") == 0)
    // else if (strcmp(host, "connectivity-check.ubuntu.com") == 0)
    // else if (strcmp(host, "www.msftconnecttest.com") == 0)
    // else if (strcmp(host, "connect.rom.miui.com") == 0)
    // else if (strcmp(host, "dns.weixin.qq.com") == 0)
    // else if (strcmp(host, "nmcheck.gnome.org") == 0)
    if ((ESP_OK == ret) && (NULL != host))
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
/**
 * @brief Post Request Handler for settings_page on web.
 * @param req Pointer to the request being responded.
 * @return - ESP_OK: Compeleted operation.
 */
esp_err_t settings_post_handler(httpd_req_t *req) // invoked when login_post is activated
{
    ESP_LOGI("ESP_SERVER", "URL:- %s", req->uri); // display the URL
    ESP_LOGE("heap-track - 1", "free-heap: %u", xPortGetFreeHeapSize());
    static settings_pass_name_t set_pass_name;
    bool ESP_receive;
    char buffer[250];
    memset(&buffer, 0, sizeof(buffer));
    size_t recv_size = (req->content_len) > sizeof(buffer) ? sizeof(buffer) : (req->content_len);
    httpd_req_recv(req, buffer, recv_size);
    buffer[249] = '\0';

    ESP_LOGI("settings_post", "Buffer : %s", buffer);

    cJSON *payload = cJSON_Parse(buffer); // returns an object holding respective [ key:value pair data ]
    if (payload)
    {
        strcpy(set_pass_name.current_password, cJSON_GetObjectItem(payload, "current_password")->valuestring);
        strcpy(set_pass_name.new_password, cJSON_GetObjectItem(payload, "new_password")->valuestring);
        strcpy(set_pass_name.confirm_password, cJSON_GetObjectItem(payload, "confirm_password")->valuestring);
        strcpy(set_pass_name.new_username, cJSON_GetObjectItem(payload, "new_username")->valuestring);
    }
    else
    {
        ESP_receive = false;
    }
    if (0 == strcmp(set_pass_name.new_password, set_pass_name.confirm_password))
    {
        ESP_receive = true;
        // read the internal password and compare with current
        nvs_handle_t set_pass_handle;
        if (ESP_OK == nvs_open("loginCreds", NVS_READWRITE, &set_pass_handle))
        {
            size_t required_size;
            nvs_get_str(set_pass_handle, "password", NULL, &required_size); // dynamic allocation and required_size => length required to hold the value.
            char *sample = malloc(required_size);
            switch (nvs_get_str(set_pass_handle, "password", sample, &required_size))
            {
            case ESP_ERR_NVS_NOT_FOUND:
                ESP_receive = false;
                ESP_LOGE("Setting_New_User_Pass_TAG", "Prev-password [To-be replaced] => not found...., Retry with Default pass :- '%s' ", default_password);
                ESP_ERROR_CHECK(nvs_set_str(set_pass_handle, "password", default_password));
                ESP_ERROR_CHECK(nvs_commit(set_pass_handle));
                break;
            case ESP_OK:
                ESP_receive = true;
                ESP_LOGW("Setting_New_User_Pass_TAG", "NVS_stored : Prev-password is => '%s' [To-be replaced] by New-password => '%s' ", sample, set_pass_name.confirm_password);
                if (0 == strcmp(set_pass_name.current_password, sample))
                { // the prev-password matches
                    ESP_ERROR_CHECK(nvs_set_str(set_pass_handle, "password", set_pass_name.confirm_password));
                    ESP_ERROR_CHECK(nvs_commit(set_pass_handle));
                    if (0 != strcmp(set_pass_name.new_username, default_username)) // incoming username shouldn't be 'default_username'
                    {
                        ESP_ERROR_CHECK(nvs_set_str(set_pass_handle, "username", set_pass_name.new_username));
                        ESP_ERROR_CHECK(nvs_commit(set_pass_handle));
                        ESP_LOGI("Setting_New_User_Pass_TAG", "Storing the New-username : '%s' in NVS storage....", set_pass_name.new_username);
                    }
                    ESP_LOGI("Setting_New_User_Pass_TAG", "Storing the New-pass : '%s' in NVS storage....", set_pass_name.confirm_password);
                }
                else
                { // the prev-password doesn't  match
                    ESP_receive = false;
                    ESP_LOGE("Setting_New_User_Pass_TAG", "WRONG current-password.... Please try again!!.");
                }
                break;
            default:
                ESP_receive = false;
                break;
            }
            if (sample != NULL)
                free(sample);
            nvs_close(set_pass_handle);
        }
    }
    else
    {
        ESP_LOGE("Setting_New_User_Pass_TAG", "New Pass: '%s' & Confirm Pass: '%s', Doesn't match", set_pass_name.new_password, set_pass_name.confirm_password);
        ESP_receive = false;
    }

    /*Preparing json data*/
    char temp_buffer[100];                   // buffer to create the json packet
    bzero(temp_buffer, sizeof(temp_buffer)); // alternative to memset
    (ESP_receive) ? (snprintf(temp_buffer, sizeof(temp_buffer), "{\"password_set_success\":%u}", 1)) : (snprintf(temp_buffer, sizeof(temp_buffer), "{\"password_set_success\":%u}", 0));
    ESP_LOGE("JSON_REPLY", "%s", temp_buffer);
    httpd_resp_set_type(req, "application/json"); // sending json data as response
    httpd_resp_sendstr(req, temp_buffer);         // chunk
    httpd_resp_send(req, NULL, 0);

    ESP_LOGE("heap-track - 2", "free-heap: %u", xPortGetFreeHeapSize());
    return ESP_OK;
}

/**
 * @brief Post Request Handler for info_page on web.
 * @param req Pointer to the request being responded.
 * @return - ESP_OK: Compeleted operation.
 */

esp_err_t info_post_handler(httpd_req_t *req) // invoked when login_post is activated
{
    ESP_LOGI("ESP_SERVER", "URL:- %s", req->uri); // display the URL
    ESP_LOGE("heap-track - 1", "free-heap: %u", xPortGetFreeHeapSize());
    bool ESP_receive;
    char buffer[20];
    memset(&buffer, 0, sizeof(buffer));
    size_t recv_size = (req->content_len) > sizeof(buffer) ? sizeof(buffer) : (req->content_len);
    httpd_req_recv(req, buffer, recv_size);
    buffer[19] = '\0';
    // ESP_LOGI("INFO_POST", "Buffer : %s", buffer);

    cJSON *payload = cJSON_Parse(buffer); // returns an object holding respective [ key:value pair data ]
    if (payload)
    {
        ESP_receive = cJSON_GetObjectItem(payload, "InfoReq")->valueint;
        ESP_LOGI("PARSE_TAG", "Info Requested => %s", (ESP_receive) ? "True" : "False");
        cJSON_Delete(payload);
    }
    else
    {
        ESP_receive = false;
    }
    if (ESP_receive)
    {
        // Preparing json data
        uint16_t val = 0;
        uint8_t chipId[6];
        memset(&chipId, 0, sizeof(chipId));
        int sysUpTime[4];
        char timestamp[30];
        memset(&sysUpTime, 0, sizeof(sysUpTime));
        struct tm Start_Time;
        esp_chip_info_t chip_info;
        const char *compile_date2 = __DATE__;
        const char *compile_time2 = __TIME__;
        char MAC[36];
        bzero(MAC, sizeof(MAC));
        MAC[35] = '\0';
        char Uptime[30];
        bzero(Uptime, sizeof(Uptime));
        Uptime[29] = '\0';

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
        sprintf(Uptime, "%dDay:%dHr:%dMin:%dSec", sysUpTime[3], sysUpTime[2], sysUpTime[1], sysUpTime[0]);

        /*boot_count*/
        nvs_handle BOOT_info;
        if (ESP_OK == nvs_open("bootVal", NVS_READWRITE, &BOOT_info))
        {
            ESP_ERROR_CHECK(nvs_get_u16(BOOT_info, "COUNT", &val));
            ESP_ERROR_CHECK(nvs_commit(BOOT_info));
            nvs_close(BOOT_info);
        }

        /*create json packet*/
        char temp_buffer[512];                   // buffer to create the json packet
        bzero(temp_buffer, sizeof(temp_buffer)); // alternative to memset
        snprintf(temp_buffer + strlen(temp_buffer), sizeof(temp_buffer) - strlen(temp_buffer), "{\"Approve\":1,\"CHIP_MODEL\":\"ESP_32\",\"CHIP_ID_MAC\":\"%s\",\"CHIP_CORES\":%d,\"FLASH_SIZE\":%d,\"HEAP_SIZE\":%d,\"FREE_DRAM\":%d,\"FREE_IRAM\":%d,\"FREE_HEAP\":%d,\"BOOT_COUNT\":%d,\"COMPILE_TIME\":\"%s\",\"UP_TIME\":\"%s\"}", (MAC), (chip_info.cores), (spi_flash_get_chip_size() / (1024 * 1024)), (FreeHeap), (DRam / 1024), (IRam / 1024), (LargestFreeHeap), (val), (timestamp), (Uptime));
        // snprintf(temp_buffer + strlen(temp_buffer), sizeof(temp_buffer) - strlen(temp_buffer), "\"CHIP_MODEL\":\"ESP_32\",");
        // snprintf(temp_buffer + strlen(temp_buffer), sizeof(temp_buffer) - strlen(temp_buffer), "\"CHIP_ID_MAC\":\"%s\",", (MAC));
        // snprintf(temp_buffer + strlen(temp_buffer), sizeof(temp_buffer) - strlen(temp_buffer), "\"CHIP_CORES\":%d,", (chip_info.cores));
        // snprintf(temp_buffer + strlen(temp_buffer), sizeof(temp_buffer) - strlen(temp_buffer), "\"FLASH_SIZE\":%d,", (spi_flash_get_chip_size() / (1024 * 1024)));
        // snprintf(temp_buffer + strlen(temp_buffer), sizeof(temp_buffer) - strlen(temp_buffer), "\"HEAP_SIZE\":%d,", (FreeHeap));
        // snprintf(temp_buffer + strlen(temp_buffer), sizeof(temp_buffer) - strlen(temp_buffer), "\"FREE_DRAM\":%d,", (DRam / 1024));
        // snprintf(temp_buffer + strlen(temp_buffer), sizeof(temp_buffer) - strlen(temp_buffer), "\"FREE_IRAM\":%d,", (IRam / 1024));
        // snprintf(temp_buffer + strlen(temp_buffer), sizeof(temp_buffer) - strlen(temp_buffer), "\"FREE_HEAP\":%d,", (LargestFreeHeap));
        // snprintf(temp_buffer + strlen(temp_buffer), sizeof(temp_buffer) - strlen(temp_buffer), "\"BOOT_COUNT\":%d,", (val));
        // snprintf(temp_buffer + strlen(temp_buffer), sizeof(temp_buffer) - strlen(temp_buffer), "\"COMPILE_TIME\":\"%s\",", (timestamp));
        // snprintf(temp_buffer + strlen(temp_buffer), sizeof(temp_buffer) - strlen(temp_buffer), "\"UP_TIME\":\"%s\"}", (Uptime));

        // sending the json packet
        httpd_resp_set_type(req, "application/json"); // sending json data as response
        httpd_resp_sendstr(req, temp_buffer);         // chunk
        httpd_resp_send(req, NULL, 0);
    }
    ESP_LOGE("heap-track - 2", "free-heap: %u", xPortGetFreeHeapSize());
    return ESP_OK;
}

/**
 * @brief Post Request Handler for relay_btn_status_refresh on web.
 * @param req Pointer to the request being responded.
 * @return - ESP_OK: Compeleted operation.
 */
esp_err_t relay_btn_refresh_handler(httpd_req_t *req) // invoked when login_post is activated
{
    ESP_LOGI("ESP_SERVER", "URL:- %s", req->uri);
    ESP_LOGE("heap-track - 1", "free-heap: %u", xPortGetFreeHeapSize());
    char buffer[20];
    memset(&buffer, 0, sizeof(buffer));
    size_t recv_size = (req->content_len) > sizeof(buffer) ? sizeof(buffer) : (req->content_len);
    httpd_req_recv(req, buffer, recv_size);
    buffer[19] = '\0';
    // ESP_LOGI("RELAY_REFRESH_BTN", "Buffer : %s", buffer);
    /********************************************* CREATING JSON ****************************************************************/
    // creating new json packet to send the success_status as reply
    char temp_buffer[256];                   // buffer to create the json packet
    bzero(temp_buffer, sizeof(temp_buffer)); // alternative to memset
    snprintf(temp_buffer, sizeof(temp_buffer), "{");
    if ((0 == Relay_Status_Value[RANDOM_UPDATE]) && (0 == Relay_Status_Value[SERIAL_UPDATE]))
    {
        for (uint8_t i = 1; i <= RELAY_UPDATE_16; i++) // get "1/0" -> relay_status [1-16]
        {
            snprintf(temp_buffer + strlen(temp_buffer), sizeof(temp_buffer) - strlen(temp_buffer), "\"Relay%u\":%u,", i, Relay_Status_Value[i]);
        }
    }
    if ((0 != Relay_Status_Value[RANDOM_UPDATE]) && (0 == Relay_Status_Value[SERIAL_UPDATE]))
    {
        snprintf(temp_buffer + strlen(temp_buffer), sizeof(temp_buffer) - strlen(temp_buffer), "\"random\":%u,", Relay_Status_Value[RANDOM_UPDATE]);
        snprintf(temp_buffer + strlen(temp_buffer), sizeof(temp_buffer) - strlen(temp_buffer), "\"serial\":%u}", 0);
    }
    else if ((0 == Relay_Status_Value[RANDOM_UPDATE]) && (1 == Relay_Status_Value[SERIAL_UPDATE]))
    {
        snprintf(temp_buffer + strlen(temp_buffer), sizeof(temp_buffer) - strlen(temp_buffer), "\"random\":%u,", 0);
        snprintf(temp_buffer + strlen(temp_buffer), sizeof(temp_buffer) - strlen(temp_buffer), "\"serial\":%u}", 1);
    }
    else
    {
        snprintf(temp_buffer + strlen(temp_buffer), sizeof(temp_buffer) - strlen(temp_buffer), "\"random\":%u,", 0);
        snprintf(temp_buffer + strlen(temp_buffer), sizeof(temp_buffer) - strlen(temp_buffer), "\"serial\":%u}", 0);
    }
    /******************************************  SENDING JSON PACKET *******************************************************************/
    httpd_resp_set_type(req, "application/json"); // sending json data as response
    httpd_resp_sendstr(req, temp_buffer);         // chunk
    httpd_resp_send(req, NULL, 0);
    ESP_LOGE("heap-track - 2", "free-heap: %u", xPortGetFreeHeapSize());
    return ESP_OK;
}

/**
 * @brief Post Request Handler for relay_page on web.
 * @param req Pointer to the request being responded.
 * @return - ESP_OK: Compeleted operation.
 */
esp_err_t relay_json_post_handler(httpd_req_t *req) // invoked when login_post is activated
{
    ESP_LOGI("ESP_SERVER", "URL:- %s", req->uri);
    ESP_LOGE("heap-track - 1", "free-heap: %u", xPortGetFreeHeapSize());
    char buffer[250];
    memset(&buffer, 0, sizeof(buffer));
    size_t recv_size = (req->content_len) > sizeof(buffer) ? sizeof(buffer) : (req->content_len);
    httpd_req_recv(req, buffer, recv_size);
    buffer[249] = '\0';
    // ESP_LOGI("RELAY_STATE_POST", "Buffer : %s", buffer);
    /********************************************* PARSING INCOMING JSON *********************************************************************************/
    // Parsing json structured credentials
    if (strstr(buffer, ":") != NULL)
    {
        // manual parsing
        // {...
        // "random":4,
        // "serial":1}
        char *pos;    // pointer to indicate the position of required member from json data
        char val[5];  // array to extract required data (in str format)
        char *endptr; // [Reference to an object of type char*] whose value is set by the function to the next character in str after the numerical value.

        pos = strstr(buffer, "random");
        if (NULL != pos)
        {
            snprintf(val, 2, "%s", (pos + strlen("random") + 2)); // {":[number]},
            Relay_inStatus_Value[RANDOM_UPDATE] = (uint8_t)strtol(val, &endptr, 10);
        }
        pos = strstr(buffer, "serial");
        if (NULL != pos)
        {
            snprintf(val, 2, "%s", (pos + strlen("serial") + 2)); // {":[number]},
            Relay_inStatus_Value[SERIAL_UPDATE] = (uint8_t)strtol(val, &endptr, 10);
        }
        if ((0 == Relay_inStatus_Value[SERIAL_UPDATE]) && (0 == Relay_inStatus_Value[RANDOM_UPDATE])) // First, storing the button status [relay1 - to - relay16]
        {
            for (uint8_t i = 1; i <= RELAY_UPDATE_16; i++)
            {
                char str[15];
                memset(str, 0, sizeof(str));
                (i > 9) ? (snprintf(str, 8, "Relay%u", i)) : (snprintf(str, 7, "Relay%u", i)); // str[] => {"Relay16+\0":_}
                pos = strstr(buffer, str);                                                     // find the string within json buffer
                snprintf(val, 2, "%s", (pos + strlen(str) + 2));                               // copy 2 characters only
                Relay_inStatus_Value[i] = (uint8_t)((strtol(val, &endptr, 10)) ? 0 : 1);       // invert logic for relay [i.e :- {ON = 0} & {OFF = 1}]
            }
        }
        ESP_LOGI("RELAY_JSON_POST_PARSE", "serial = %d , random = %d", Relay_inStatus_Value[SERIAL_UPDATE], Relay_inStatus_Value[RANDOM_UPDATE]);
        /******************************************* STORING NEW RELAY STATUS ******************************************************************/
        // Storing button status in nvs-storage
        nvs_handle_t nvs_relay;
        if (ESP_OK == nvs_open("Relay_Status", NVS_READWRITE, &nvs_relay))
        {
            if ((0 == Relay_inStatus_Value[RANDOM_UPDATE]) && (0 == Relay_inStatus_Value[SERIAL_UPDATE]))
            {
                for (uint8_t i = 1; i <= 16; i++)
                {
                    char str[10];
                    memset(str, 0, sizeof(str));
                    sprintf(str, "Relay%u", i);
                    nvs_set_u8(nvs_relay, str, Relay_inStatus_Value[i]);
                    nvs_commit(nvs_relay);
                }
            }
            if ((0 != Relay_inStatus_Value[RANDOM_UPDATE]) && (0 == Relay_inStatus_Value[SERIAL_UPDATE]))
            {
                nvs_set_u8(nvs_relay, "random", Relay_inStatus_Value[RANDOM_UPDATE]); // random => [0/0ff] vs [1/ON , 2/ON , 3/ON , 4/ON]
                nvs_commit(nvs_relay);
                nvs_set_u8(nvs_relay, "serial", 0); // serial => [0/0ff] vs [1/ON]
                nvs_commit(nvs_relay);
            }
            else if ((1 == Relay_inStatus_Value[SERIAL_UPDATE]) && (0 == Relay_inStatus_Value[RANDOM_UPDATE]))
            {
                nvs_set_u8(nvs_relay, "random", 0); // random => [0/0ff] vs [1/ON , 2/ON , 3/ON , 4/ON]
                nvs_commit(nvs_relay);
                nvs_set_u8(nvs_relay, "serial", 1); // serial => [0/0ff] vs [1/ON]
                nvs_commit(nvs_relay);
            }
            else
            {
                nvs_set_u8(nvs_relay, "random", 0); // random => [0/0ff] vs [1/ON , 2/ON , 3/ON , 4/ON]
                nvs_commit(nvs_relay);
                nvs_set_u8(nvs_relay, "serial", 0); // serial => [0/0ff] vs [1/ON]
                nvs_commit(nvs_relay);
            }
            nvs_close(nvs_relay);
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // delay for stability
        /******************************************** TURN ON/OFF GPIO_PINS *****************************************************************/
        Activate_Relays();                    // retrieve the stored data, Invoke the relay switches and generate "update_success_status" values
        vTaskDelay(100 / portTICK_PERIOD_MS); // delay for stability
        /********************************************* CREATING JSON ****************************************************************/
        // creating new json packet to send the success_status as reply

        char temp_buffer[512];
        bzero(temp_buffer, sizeof(temp_buffer));
        snprintf(temp_buffer, sizeof(temp_buffer), "{\"random_update_success\":%u,\"serial_update_success\":%u", Relay_Update_Success[RANDOM_UPDATE], Relay_Update_Success[SERIAL_UPDATE]);
        if ((0 == Relay_inStatus_Value[SERIAL_UPDATE]) && (0 == Relay_inStatus_Value[RANDOM_UPDATE]))
        {
            for (uint8_t i = 1; i <= RELAY_UPDATE_16; i++) // get "1/0" -> relay_status [1-16]
            {
                snprintf(temp_buffer + strlen(temp_buffer), sizeof(temp_buffer) - strlen(temp_buffer), ",\"Relay%u_update_success\":%u", i, Relay_Update_Success[i]);
            }
        }
        snprintf(temp_buffer + strlen(temp_buffer), sizeof(temp_buffer) - strlen(temp_buffer), "}");

        /****************************************** SENDING json reply packet *******************************************************************/
        // ESP_LOGW("RELAY_JSON_REPLY", "%s", temp_buffer);
        httpd_resp_set_type(req, "application/json"); // sending json data as response
        httpd_resp_sendstr(req, temp_buffer);         // chunk
        httpd_resp_send(req, NULL, 0);
        /***********************************************************************************************************************/
    }
    else
    {
        ESP_LOGE("RELAY_INPUT", "Incomming JSON_packet not proper!!");
        httpd_resp_set_status(req, "204 NO CONTENT");
        httpd_resp_send(req, NULL, 0);
    }
    ESP_LOGE("heap-track - 2", "free-heap: %u", xPortGetFreeHeapSize());
    return ESP_OK;
}

/**
 * @brief Post Request Handler for restart_page on web.
 * @param req Pointer to the request being responded.
 * @return - ESP_OK: Compeleted operation.
 */
esp_err_t restart_handler(httpd_req_t *req) // invoked when login_post is activated
{
    ESP_LOGI("ESP_SERVER", "URL:- %s", req->uri); // display the URL
    ESP_LOGE("heap-track - 1", "free-heap: %u", xPortGetFreeHeapSize());
    bool ESP_receive;
    char buffer[20]; // this stores username and password into buffer //
    memset(&buffer, 0, sizeof(buffer));
    size_t recv_size = (req->content_len) > sizeof(buffer) ? sizeof(buffer) : (req->content_len);
    httpd_req_recv(req, buffer, recv_size);
    buffer[19] = '\0';
    // ESP_LOGI("RESTART_TAG", "Buffer : %s", buffer);
    if (NULL != buffer)
    {
        cJSON *payload = cJSON_Parse(buffer); // returns an object holding respective [ key:value pair data ]
        if (payload)
        {
            ESP_receive = cJSON_GetObjectItem(payload, "restart")->valueint;
            ESP_LOGI("RESTART_PARSE_TAG", "Restart Requested => %s", (ESP_receive) ? "True" : "False");
            cJSON_Delete(payload);
        }
        else
        {
            ESP_receive = false;
        }
        /*Preparing json data*/
        char temp_buffer[100];
        bzero(temp_buffer, sizeof(temp_buffer));
        (ESP_receive) ? (snprintf(temp_buffer, sizeof(temp_buffer), "{\"restart_successful\":%u,", 1)) : (snprintf(temp_buffer, sizeof(temp_buffer), "{\"restart_successful\":%u,", 0));
        snprintf(temp_buffer + strlen(temp_buffer), sizeof(temp_buffer) - strlen(temp_buffer), "\"IP_addr3\":%u}", (uint8_t)((STA_ADDR3) ? STA_ADDR3 : 0));

        ESP_LOGE("RESTART_JSON_REPLY", "%s", temp_buffer);
        httpd_resp_set_type(req, "application/json"); // sending json data as response
        httpd_resp_sendstr(req, temp_buffer);         // chunk
        httpd_resp_send(req, NULL, 0);
        ESP_LOGE("heap-track - 2", "free-heap: %u", xPortGetFreeHeapSize());

        if (ESP_receive)
        {
            // for (int i = 2; i > 0; i--)
            // {
            ESP_LOGW("RESTART_TAG", "Restarting... NOW.");
            vTaskDelay(500 / portTICK_PERIOD_MS);
            // }
            response.approve = false;
            esp_restart();
        }
    }
    else
    {
        ESP_LOGE("RELAY_INPUT", "Incomming JSON_packet not proper!!");
        httpd_resp_set_status(req, "204 NO CONTENT");
        httpd_resp_send(req, NULL, 0);
        ESP_LOGE("heap-track - 2", "free-heap: %u", xPortGetFreeHeapSize());
    }
    return ESP_OK;
}

/**
 * @brief Post Request Handler for NDS_login_page on web.
 * @param req Pointer to the request being responded.
 * @return - ESP_OK: Compeleted operation.
 */
esp_err_t login_auth_handler(httpd_req_t *req) // invoked when login_post is activated
{
    ESP_LOGI("ESP_SERVER", "URL:- %s", req->uri); // display the URL
    ESP_LOGE("heap-track - 1", "free-heap: %u", xPortGetFreeHeapSize());
    static auth_t cred; // cred obj // stores data into the memory even after this function terminates
    char buffer[100];   // this stores username and password into buffer //
    memset(&buffer, 0, sizeof(buffer));
    size_t recv_size = (req->content_len) > sizeof(buffer) ? sizeof(buffer) : (req->content_len);
    httpd_req_recv(req, buffer, recv_size);
    buffer[99] = '\0';
    ESP_LOGW("login_data_post", "Buffer : %s", buffer);
    if (NULL != buffer)
    {
        cJSON *payload = cJSON_Parse(buffer); // returns an object holding respective [ key:value pair data ]
        if (payload)
        {
            strcpy(cred.username, cJSON_GetObjectItem(payload, "username")->valuestring);
            strcpy(cred.password, cJSON_GetObjectItem(payload, "password")->valuestring);
            ESP_LOGI("PARSE_TAG", "USERNAME = %s", cred.username);
            ESP_LOGI("PARSE_TAG", "PASSWORD = %s", cred.password);
            cJSON_Delete(payload);
        }

        // /* manual parsing*/
        // char *U = strtok(buffer, ","); // get the username_token [8-32 limit]
        // char *P = strtok(NULL, "}");   // get the password_token [8-32 limit]
        // ESP_LOGI("Parse_tag_buffer", "%s", U);
        // ESP_LOGI("Parse_tag_buffer", "%s", P);
        // char *U_pos_start = (strstr(U, ":\"") + 2);              // starting ptr to ( "{username":"[x]_____", )
        // char *U_pos_end = strrchr(U, '\"');                      // ending ptr to ( "{username":"_____[x]", )
        // size_t U_length = (size_t)((U_pos_end) - (U_pos_start)); // find the length
        // char *P_pos_start = (strstr(P, ":\"") + 2);              // starting ptr to ( "password":"[x]_____" )
        // char *P_pos_end = strrchr(P, '\"');                      // ending ptr to ( "password":"_____[x]" )
        // size_t P_length = (size_t)((P_pos_end) - (P_pos_start)); // find the length
        // snprintf(cred.username, U_length + 1, "%s", U_pos_start); // 32char + '\0'
        // snprintf(cred.password, P_length + 1, "%s", P_pos_start); // 32char + '\0'
        // ESP_LOGI("Manual_Parse_tag", "%s", cred.username);
        // ESP_LOGI("Manual_Parse_tag", "%s", cred.password);

        // first decide the login_mode; response.approve => true.
        esp_err_t res = (login_cred(&cred));
        if (res > 0) // data not found in nvs
        {
            ESP_LOGI("AUTH_TAG", "....Init_Mode....[ADMIN/ADMIN]");
            if ((strcmp(cred.username, default_username) == 0) && (strcmp(cred.password, default_password) == 0))
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
        else // data found in nvs
        {
            if (res == ESP_OK) // esp_ok
            {
                response.approve = true;
                ESP_LOGW("AUTH_TAG", "USERNAME & PASSWORD........ Approved");
            }
            else if (res == ESP_FAIL) // esp_fail
            {
                response.approve = false;
                ESP_LOGE("AUTH_TAG", "Please reload into same IP. Incorrect username/password");
                httpd_resp_set_status(req, "204 NO CONTENT");
                httpd_resp_send(req, NULL, 0);
                return ESP_OK;
            }
        }

        /* Sending json data */
        char temp_buffer[50];
        bzero(temp_buffer, sizeof(temp_buffer));
        snprintf(temp_buffer, sizeof(temp_buffer), "{\"approve\":%u}", (uint8_t)response.approve);
        ESP_LOGI("LOGIN_JSON_REPLY", "%s", temp_buffer);
        httpd_resp_set_type(req, "application/json"); // sending json data as response
        httpd_resp_sendstr(req, temp_buffer);         // chunk
        httpd_resp_send(req, NULL, 0);
    }
    else
    {
        ESP_LOGE("RELAY_INPUT", "Incomming JSON_packet not proper!!");
        httpd_resp_set_status(req, "204 NO CONTENT");
        httpd_resp_send(req, NULL, 0);
    }
    ESP_LOGE("heap-track - 2", "free-heap: %u", xPortGetFreeHeapSize());
    return ESP_OK;
}

/**
 * @brief Post Request Handler for assets on web.
 * @param req Pointer to the request being responded.
 * @return - ESP_OK: Compeleted operation.
 */
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
    if (0 == strcmp(ext, ".css"))
    {
        httpd_resp_set_type(req, "text/css");
    }
    if (0 == strcmp(ext, ".js"))
    {
        httpd_resp_set_type(req, "text/javascript");
    }

    // handle other files
    FILE *file = fopen(path, "r"); // as string
    if (NULL == file)
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

/**
 * @brief Post Request Handler for image on web.
 * @param req Pointer to the request being responded.
 * @return - ESP_OK: Compeleted operation.
 */
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

    // routing the file path
    char path[600];
    sprintf(path, "/spiffs%s", req->uri); // eg: /spiffs/assets/script.js
    char *ext = strrchr(path, '.');       // extension :-  path/.js,css,png
    // for image extensions
    if (0 == strcmp(ext, ".jpg"))
    {
        httpd_resp_set_type(req, "image/jpeg");
    }
    if (0 == strcmp(ext, ".jpeg")) // for image extensions
    {
        httpd_resp_set_type(req, "image/jpeg");
    }
    if (0 == strcmp(ext, ".png")) // for image extensions
    {
        httpd_resp_set_type(req, "image/png");
    }

    FILE *file = fopen(path, "rb"); // binary read // handle other files
    if (NULL == file)
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
    esp_vfs_spiffs_unregister(NULL); // unmount // avoid memory leaks
    return ESP_OK;
}

/******************** local_wifi connection function: [ESP => AP TO STA] *******************/
/**
 * @brief Task that clear TCP/IP stack and re-initialized the ESP32 in STA_mode.
 */
void connect_to_local_AP(void *params)
{
    // vTaskDelay(100 / portTICK_PERIOD_MS);
    ap_config_t *ap_config = (ap_config_t *)params; // type casting to ap_config_t

    // wifi_destroy_netif();                               // destroy netif infos [like ip,mac..] for wifi_AP_mode
    wifi_disconnect();     // disconnect the wifi_AP_mode
    dns_hijack_srv_stop(); // just making sure to close
    ESP_LOGE("PARSE_SSID", "%s", ap_config->local_ssid);
    ESP_LOGE("PARSE_PASS", "%s", ap_config->local_pass);

    if (wifi_connect_sta(ap_config->local_ssid, ap_config->local_pass, 20000) == ESP_OK) // if the local ssid/pass doesn't match
    {
        ESP_LOGI("AP_to_STA_TAG", "CONNECTED TO New_SSID... > Saving cred of %s in NVS... ", ap_config->local_ssid);
        // nvs_flash_init(); // no harm in re_initing the nvs
        nvs_handle nvs_write_handle;
        if (ESP_OK == nvs_open("wifiCreds", NVS_READWRITE, &nvs_write_handle)) // namespace => local_APs
        {
            ESP_ERROR_CHECK(nvs_set_str(nvs_write_handle, "store_ssid", ap_config->local_ssid)); // key = store_ssid ; value = structure{}
            ESP_ERROR_CHECK(nvs_commit(nvs_write_handle));
            ESP_ERROR_CHECK(nvs_set_str(nvs_write_handle, "store_pass", ap_config->local_pass)); // key = store_pass ; value = structure{}
            ESP_ERROR_CHECK(nvs_commit(nvs_write_handle));
            nvs_close(nvs_write_handle);
        }

        // start http server for login after [ ESP => STA ]
        ESP_LOGE("AP_TO_STA_RESTART", "STA Connection successful");
        for (uint8_t a = 3; a > 0; a--)
        {
            ESP_LOGE("AP_TO_STA_RESTART", "Restarting STA mode in ...%d", a);
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        wifi_disconnect();
        vTaskDelay(pdMS_TO_TICKS(5));
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

/**
 * @brief Function to switch the ESP32 from AP_mode to STA_mode.
 *
 * @return - ESP_OK: Compeleted operation.
 */
esp_err_t AP_TO_STA(httpd_req_t *req)
{
    ESP_LOGI("ESP_SERVER", "URL:- %s", req->uri); // display the URL
    ESP_LOGE("heap-track - 1", "free-heap: %u", xPortGetFreeHeapSize());
    static ap_config_t ap_config; // required to stay in memory

    char buffer[100];
    memset(&buffer, 0, sizeof(buffer));
    httpd_req_recv(req, buffer, req->content_len); // "local_ssid=xxx\r\nlocal_pass=xxxx\r\n"// revieve the contents inside buf
    ESP_LOGW("portal_data", "Buffer : %s", buffer);

    /*Parsing Incomming Data*/
    cJSON *payload = cJSON_Parse(buffer);
    if (payload)
    {
        strcpy(ap_config.local_ssid, cJSON_GetObjectItem(payload, "local_ssid")->valuestring); // this "ap_config" structure is passed to connect to new ssid
        strcpy(ap_config.local_pass, cJSON_GetObjectItem(payload, "local_pass")->valuestring);
        cJSON_Delete(payload);
    }

    char temp_buffer[50];
    bzero(temp_buffer, sizeof(temp_buffer));
    snprintf(temp_buffer, sizeof(temp_buffer), "{\"IP_addr3\":%u}", (uint8_t)((STA_ADDR3) ? STA_ADDR3 : 0));

    ESP_LOGE("JSON_REPLY", "%s", temp_buffer);
    httpd_resp_set_type(req, "application/json"); // sending json data as response
    httpd_resp_sendstr(req, temp_buffer);         // chunk
    httpd_resp_send(req, NULL, 0);

    vTaskDelay(200 / portTICK_PERIOD_MS);
    xTaskCreate(connect_to_local_AP, "connect_to_local_ap", 4096, &ap_config, 1, NULL);
    ESP_LOGE("heap-track - 2", "free-heap: %u", xPortGetFreeHeapSize());
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
        cJSON_AddNumberToObject(element, "rssi", wifi_ap_record[i].rssi);         // adding rssi to json obj
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

/**
 * @brief Function to configure & initailized Server.
 */
void Connect_Portal()
{ // only invoke this config portion if server is reset
    if (NULL == server)
    {
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        config.max_uri_handlers = 16;
        // config.max_open_sockets = 5,
        config.recv_wait_timeout = 7,
        config.send_wait_timeout = 7,
        config.uri_match_fn = httpd_uri_match_wildcard;
        ESP_ERROR_CHECK(httpd_start(&server, &config));
        ESP_LOGW("Server_TAG", "Server Start Successful...");

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

/**
 * @brief Function to initailized and start Server_AP_mode.
 */
void http_server_ap_mode(void)
{
    httpd_unregister_uri_handler(server, "/", HTTP_GET);
    httpd_unregister_uri_handler(server, "/info", HTTP_GET);
    httpd_unregister_uri_handler(server, "/dashboard", HTTP_GET);
    httpd_unregister_uri_handler(server, "/relay", HTTP_GET);
    httpd_unregister_uri_handler(server, "/settings", HTTP_GET);
    httpd_unregister_uri_handler(server, "/info_post", HTTP_POST);
    httpd_unregister_uri_handler(server, "/settings_post", HTTP_POST);
    httpd_unregister_uri_handler(server, "/restart", HTTP_POST);
    httpd_unregister_uri_handler(server, "/login_auth_post", HTTP_POST);
    httpd_unregister_uri_handler(server, "/relay_btn_refresh", HTTP_POST);
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

/**
 * @brief Function to initailized and start Server_STA_mode.
 */
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
    httpd_register_uri_handler(server, &get_info_url);

    httpd_uri_t dashboard_url = {
        .uri = "/dashboard",
        .method = HTTP_GET,
        .handler = dashboard_handler};
    httpd_register_uri_handler(server, &dashboard_url);

    httpd_uri_t relay_url = {
        .uri = "/relay",
        .method = HTTP_GET,
        .handler = relay_handler};
    httpd_register_uri_handler(server, &relay_url);

    httpd_uri_t settings_url = {
        .uri = "/settings",
        .method = HTTP_GET,
        .handler = settings_handler};
    httpd_register_uri_handler(server, &settings_url);

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

    httpd_uri_t settings_post_url = {
        .uri = "/settings_post",
        .method = HTTP_POST,
        .handler = settings_post_handler};
    httpd_register_uri_handler(server, &settings_post_url);

    httpd_uri_t relay_btn_refresh_url = {
        .uri = "/relay_btn_refresh",
        .method = HTTP_POST,
        .handler = relay_btn_refresh_handler};
    httpd_register_uri_handler(server, &relay_btn_refresh_url);

    httpd_uri_t relay_json_post_url = {
        .uri = "/relay_json_post",
        .method = HTTP_POST,
        .handler = relay_json_post_handler};
    httpd_register_uri_handler(server, &relay_json_post_url);
}

/**
 * @brief Function to Start DNS Server in AP_mode.
 */
void start_dns_server(void)
{
    ip4_addr_t resolve_ip;
    inet_pton(AF_INET, "192.168.1.1", &resolve_ip);
    if (dns_hijack_srv_start(resolve_ip) == ESP_OK)
    {
        ESP_LOGE("DNS_TAG", "DNS server started...");
    }
}

/*******************************************************************************
 *                          End of File
 *******************************************************************************/
