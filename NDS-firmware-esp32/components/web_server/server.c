/*******************************************************************************
 *                          Include Files
 *******************************************************************************/
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
#include "cJSON.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "connect.h"
#include "dns_hijack_srv.h"
#include "netdb.h"

/*******************************************************************************
 *                          Static Data Definitions
 *******************************************************************************/
/* login cred index */
#define username_index 0
#define password_index 1
/* threshold memory in KB*/
#define THRESHOLD_HEAP 100
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
 * @brief function to inspect the remaining esp_heap_memory.
 *
 * @return - ESP_OK: esp_memory remaining is above required threshold memory size.
 * @return - ESP_FAIL: esp_memory remaining is below required threshold memory size.
 */
static esp_err_t esp_memory_refresh()
{
    ESP_LOGE("ESP_HEAP_SIZE", "%d", esp_get_free_heap_size());
    if ((int)esp_get_free_heap_size() <= THRESHOLD_HEAP) // threshold in kB
        return ESP_FAIL;
    else
        return ESP_OK;
}

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
        if ((index == 0))
        {
            if (strcmp(Data->username, sample) == 0)
            {
                ESP_LOGW("LOGIN_TAG", "USERNAME ........ Approved");
            }
            else
            {
                ESP_LOGE("LOGIN_TAG", "WRONG USERNAME ENTRY ........ Not Approved");
                err = ESP_FAIL;
            }
        }
        else if ((index == 1))
        {
            if (strcmp(Data->password, sample) == 0)
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
    esp_err_t err1, err2;
    ESP_ERROR_CHECK(nvs_open("loginCreds", NVS_READWRITE, &login)); // namespace => loginCreds
    err1 = inspect_login_data(Data, &login, "username", username_index);
    err2 = inspect_login_data(Data, &login, "password", password_index);
    nvs_close(login);

    if ((err1 == 1) && (err2 == 1))
    {
        ESP_LOGE("Login_cred_change", ".... RETURNS '1'[i.e. no creds; storing default] ....");
        return 1;
    }
    else if ((err1 == ESP_OK) && (err2 == ESP_OK))
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
    // query_mdns_hosts_async(local_server_name);
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
/**
 * @brief Post Request Handler for settings_page on web.
 * @param req Pointer to the request being responded.
 * @return - ESP_OK: Compeleted operation.
 */
esp_err_t settings_post_handler(httpd_req_t *req) // invoked when login_post is activated
{
    ESP_LOGI("ESP_SERVER", "URL:- %s", req->uri); // display the URL
    static settings_pass_name_t set_pass_name;
    bool ESP_receive;
    char buffer[250];
    memset(&buffer, 0, sizeof(buffer));
    httpd_req_recv(req, buffer, req->content_len); // size_t recv_size = (req->content_len) > sizeof(buffer) ? sizeof(buffer) : (req->content_len);
    // ESP_LOGW("settings_post", "Buffer : %s", buffer);

    cJSON *payload = cJSON_Parse(buffer); // returns an object holding respective [ key:value pair data ]
    strcpy(set_pass_name.current_password, cJSON_GetObjectItem(payload, "current_password")->valuestring);
    strcpy(set_pass_name.new_password, cJSON_GetObjectItem(payload, "new_password")->valuestring);
    strcpy(set_pass_name.confirm_password, cJSON_GetObjectItem(payload, "confirm_password")->valuestring);
    strcpy(set_pass_name.new_username, cJSON_GetObjectItem(payload, "new_username")->valuestring);

    if (strcmp(set_pass_name.new_password, set_pass_name.confirm_password) == 0)
    {
        ESP_receive = true;
        // read the internal password and compare with current
        nvs_handle_t set_pass_handle;
        ESP_ERROR_CHECK(nvs_open("loginCreds", NVS_READWRITE, &set_pass_handle));
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
            if (strcmp(set_pass_name.current_password, sample) == 0)
            { // the prev-password matches
                ESP_ERROR_CHECK(nvs_set_str(set_pass_handle, "password", set_pass_name.confirm_password));
                ESP_ERROR_CHECK(nvs_commit(set_pass_handle));
                if (strcmp(set_pass_name.new_username, default_username) != 0) // incoming username shouldn't be 'default_username'
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
    else
    {
        ESP_LOGE("Setting_New_User_Pass_TAG", "New Pass: '%s' & Confirm Pass: '%s', Doesn't match", set_pass_name.new_password, set_pass_name.confirm_password);
        ESP_receive = false;
    }
    // Preparing json data
    cJSON *JSON_data = cJSON_CreateObject();
    (ESP_receive) ? (cJSON_AddNumberToObject(JSON_data, "password_set_success", 1)) : (cJSON_AddNumberToObject(JSON_data, "password_set_success", 0));
    char *string_json = cJSON_Print(JSON_data);
    ESP_LOGE("JSON_REPLY", "%s", string_json);
    httpd_resp_set_type(req, "application/json"); // sending json data as response
    httpd_resp_sendstr(req, string_json);         // chunk
    httpd_resp_send(req, NULL, 0);
    free(string_json);
    cJSON_free(JSON_data);

    /*
    // now check the reamining heap_size ; if lower than threshold , RESTART ESP
    if (esp_memory_refresh() == ESP_FAIL)
    {
        ESP_LOGE("HEAP_MEMORY_LOW ", ".................RESTARTING ESP....................");
        esp_restart();
    }
    */
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
    bool ESP_receive;
    char buffer[5];
    memset(&buffer, 0, sizeof(buffer));
    httpd_req_recv(req, buffer, req->content_len); // size_t recv_size = (req->content_len) > sizeof(buffer) ? sizeof(buffer) : (req->content_len);
    // ESP_LOGW("Info_post", "Buffer : %s", buffer);

    cJSON *payload = cJSON_Parse(buffer); // returns an object holding respective [ key:value pair data ]
    ESP_receive = cJSON_GetObjectItem(payload, "InfoReq")->valueint;
    ESP_LOGI("PARSE_TAG", "Info Requested => %s", (ESP_receive) ? "True" : "False");
    cJSON_Delete(payload);
    if (ESP_receive)
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

        /*
        // now check the reamining heap_size ; if lower than threshold , RESTART ESP
        if (esp_memory_refresh() == ESP_FAIL)
        {
            ESP_LOGE("HEAP_MEMORY_LOW ", ".................RESTARTING ESP....................");
            esp_restart();
        }
        */
    }
    return ESP_OK;
}

/**
 * @brief Post Request Handler for relay_btn_status_refresh on web.
 * @param req Pointer to the request being responded.
 * @return - ESP_OK: Compeleted operation.
 */
esp_err_t relay_btn_refresh_handler(httpd_req_t *req) // invoked when login_post is activated
{
    char buffer[5];
    memset(&buffer, 0, sizeof(buffer));
    httpd_req_recv(req, buffer, req->content_len); // size_t recv_size = (req->content_len) > sizeof(buffer) ? sizeof(buffer) : (req->content_len);
    // ESP_LOGI("RELAY_REFRESH_BTN", "Buffer : %s", buffer);
    /********************************************* CREATING JSON ****************************************************************/
    // creating new json packet to send the success_status as reply
    cJSON *JSON_data = cJSON_CreateObject();
    if (Relay_Status_Value[RANDOM_UPDATE] == 0 && Relay_Status_Value[SERIAL_UPDATE] == 0)
        for (uint8_t i = 1; i <= RELAY_UPDATE_16; i++) // get "1/0" -> relay_status [1-16]
        {
            char *str = (char *)malloc(sizeof("Relay") + 2);
            memset(str, 0, sizeof("Relay") + 2);
            sprintf(str, "Relay%u", i);
            cJSON_AddNumberToObject(JSON_data, str, (Relay_Status_Value[i])); // sending back the inverse to web browser (avoid confusion) ; R_ON => 1 ; R_OFF = 0
            free(str);
        }
    if (Relay_Status_Value[RANDOM_UPDATE] != 0 && Relay_Status_Value[SERIAL_UPDATE] == 0)
    {
        cJSON_AddNumberToObject(JSON_data, "random", Relay_Status_Value[RANDOM_UPDATE]); // random => [0/0ff] vs [1/ON , 2/ON , 3/ON , 4/ON]
        cJSON_AddNumberToObject(JSON_data, "serial", 0);                                 // serial => [0/0ff] vs [1/ON]
    }
    else if (Relay_Status_Value[SERIAL_UPDATE] == 1 && Relay_Status_Value[RANDOM_UPDATE] == 0)
    {
        cJSON_AddNumberToObject(JSON_data, "random", 0); // random => [0/0ff] vs [1/ON , 2/ON , 3/ON , 4/ON]
        cJSON_AddNumberToObject(JSON_data, "serial", 1); // serial => [0/0ff] vs [1/ON]
    }
    else
    {
        cJSON_AddNumberToObject(JSON_data, "random", 0); // random => [0/0ff] vs [1/ON , 2/ON , 3/ON , 4/ON]
        cJSON_AddNumberToObject(JSON_data, "serial", 0); // serial => [0/0ff] vs [1/ON]
    }
    char *string_json = cJSON_Print(JSON_data);
    // ESP_LOGE("JSON_REPLY_BTN_REFRESH", "%s", string_json);
    /******************************************  SENDING JSON PACKET *******************************************************************/
    httpd_resp_set_type(req, "application/json"); // sending json data as response
    httpd_resp_sendstr(req, string_json);         // chunk
    httpd_resp_send(req, NULL, 0);
    free(string_json);
    cJSON_free(JSON_data);
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
    char buffer[250];
    memset(&buffer, 0, sizeof(buffer));
    httpd_req_recv(req, buffer, req->content_len); // size_t recv_size = (req->content_len) > sizeof(buffer) ? sizeof(buffer) : (req->content_len);
    // ESP_LOGW("RELAY_STATE_POST", "Buffer : %s", buffer);
    /********************************************* PARSING INCOMING JSON *********************************************************************************/
    // Parsing json structured credentials
    if (strstr(buffer, ":") != NULL)
    {
        // parsing json data into Relay_status_array
        cJSON *payload = cJSON_Parse(buffer);                                                     // returns an object holding respective [ key:value pair data ]
        Relay_inStatus_Value[SERIAL_UPDATE] = (cJSON_GetObjectItem(payload, "serial")->valueint); // parse serial
        Relay_inStatus_Value[RANDOM_UPDATE] = (cJSON_GetObjectItem(payload, "random")->valueint); // parse random
        if (Relay_inStatus_Value[SERIAL_UPDATE] == 0 && Relay_inStatus_Value[RANDOM_UPDATE] == 0) // First, storing the button status [relay1 - to - relay16]
        {
            for (uint8_t i = 1; i <= 16; i++)
            {
                char *str = (char *)malloc(sizeof("Relay") + 2);
                memset(str, 0, sizeof("Relay") + 2);
                sprintf(str, "Relay%u", i);
                Relay_inStatus_Value[i] = (cJSON_GetObjectItem(payload, str)->valueint) ? 0 : 1; // invert logic for relay [i.e :- {ON = 0} & {OFF = 1}]
                free(str);
            }
        }
        cJSON_Delete(payload);
        ESP_LOGW("RELAY_JSON_POST_PARSE", "serial = %d , random = %d", Relay_inStatus_Value[SERIAL_UPDATE], Relay_inStatus_Value[RANDOM_UPDATE]);
        /******************************************* STORING NEW RELAY STATUS ******************************************************************/
        // Storing button status in nvs-storage
        nvs_handle_t nvs_relay;
        nvs_open("Relay_Status", NVS_READWRITE, &nvs_relay);
        if (Relay_inStatus_Value[RANDOM_UPDATE] == 0 && Relay_inStatus_Value[SERIAL_UPDATE] == 0)
        {
            for (uint8_t i = 1; i <= 16; i++)
            {
                char *str = (char *)malloc(sizeof("Relay") + 2);
                memset(str, 0, sizeof("Relay") + 2);
                sprintf(str, "Relay%u", i);
                nvs_set_u8(nvs_relay, str, Relay_inStatus_Value[i]);
                nvs_commit(nvs_relay);
                free(str);
            }
        }

        if (Relay_inStatus_Value[RANDOM_UPDATE] != 0 && Relay_inStatus_Value[SERIAL_UPDATE] == 0)
        {
            nvs_set_u8(nvs_relay, "random", Relay_inStatus_Value[RANDOM_UPDATE]); // random => [0/0ff] vs [1/ON , 2/ON , 3/ON , 4/ON]
            nvs_commit(nvs_relay);
            nvs_set_u8(nvs_relay, "serial", 0); // serial => [0/0ff] vs [1/ON]
            nvs_commit(nvs_relay);
            nvs_close(nvs_relay);
        }
        else if (Relay_inStatus_Value[SERIAL_UPDATE] == 1 && Relay_inStatus_Value[RANDOM_UPDATE] == 0)
        {
            nvs_set_u8(nvs_relay, "random", 0); // random => [0/0ff] vs [1/ON , 2/ON , 3/ON , 4/ON]
            nvs_commit(nvs_relay);
            nvs_set_u8(nvs_relay, "serial", 1); // serial => [0/0ff] vs [1/ON]
            nvs_commit(nvs_relay);
            nvs_close(nvs_relay);
        }
        else
        {
            nvs_set_u8(nvs_relay, "random", 0); // random => [0/0ff] vs [1/ON , 2/ON , 3/ON , 4/ON]
            nvs_commit(nvs_relay);
            nvs_set_u8(nvs_relay, "serial", 0); // serial => [0/0ff] vs [1/ON]
            nvs_commit(nvs_relay);
            nvs_close(nvs_relay);
        }

        vTaskDelay(pdMS_TO_TICKS(10)); // delay for stability
        /******************************************** TURN ON/OFF GPIO_PINS *****************************************************************/
        xSemaphoreGive(xSEMA);                // retrieve the stored data, Invoke the relay switches and generate "update_success_status" values
        vTaskDelay(100 / portTICK_PERIOD_MS); // delay for stability
        /********************************************* CREATING JSON ****************************************************************/
        // creating new json packet to send the success_status as reply
        cJSON *JSON_data = cJSON_CreateObject();
        cJSON_AddNumberToObject(JSON_data, "random_update_success", Relay_Update_Success[RANDOM_UPDATE]);
        cJSON_AddNumberToObject(JSON_data, "serial_update_success", Relay_Update_Success[SERIAL_UPDATE]);
        if (Relay_inStatus_Value[SERIAL_UPDATE] == 0 && Relay_inStatus_Value[RANDOM_UPDATE] == 0)
        {
            for (uint8_t i = 1; i <= RELAY_UPDATE_16; i++) // get "1/0" -> relay_status [1-16]
            {
                char *str = (char *)malloc(sizeof("Relay_update_success") + 2);
                memset(str, 0, sizeof("Relay_update_success") + 2);
                sprintf(str, "Relay%u_update_success", i);
                cJSON_AddNumberToObject(JSON_data, str, Relay_Update_Success[i]);
                free(str);
            }
        }
        /****************************************** SENDING json reply packet *******************************************************************/
        char *string_json = cJSON_Print(JSON_data);
        // ESP_LOGE("JSON_REPLY", "%s", string_json);
        httpd_resp_set_type(req, "application/json"); // sending json data as response
        httpd_resp_sendstr(req, string_json);         // chunk
        httpd_resp_send(req, NULL, 0);
        free(string_json);
        cJSON_free(JSON_data);
        /***********************************************************************************************************************/
    }
    else
    {
        ESP_LOGE("RELAY_INPUT", "Incomming JSON_packet not proper!!");
        httpd_resp_set_status(req, "204 NO CONTENT");
        httpd_resp_send(req, NULL, 0);
    }
    /*
    // now check the reamining heap_size ; if lower than threshold , RESTART ESP
    if (esp_memory_refresh() == ESP_FAIL)
    {
        ESP_LOGE("HEAP_MEMORY_LOW ", ".................RESTARTING ESP....................");
        esp_restart();
    }
    */
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
    bool Esp_receive;
    char buffer[5]; // this stores username and password into buffer //
    memset(&buffer, 0, sizeof(buffer));
    httpd_req_recv(req, buffer, req->content_len); // size_t recv_size = (req->content_len) > sizeof(buffer) ? sizeof(buffer) : (req->content_len);
    // ESP_LOGW("restart_post", "Buffer : %s", buffer);

    cJSON *payload = cJSON_Parse(buffer); // returns an object holding respective [ key:value pair data ]
    Esp_receive = cJSON_GetObjectItem(payload, "restart")->valueint;
    ESP_LOGI("PARSE_TAG", "Restart Requested => %s", (Esp_receive) ? "True" : "False");
    cJSON_Delete(payload);

    // Preparing json data
    cJSON *JSON_data = cJSON_CreateObject();
    (Esp_receive) ? cJSON_AddNumberToObject(JSON_data, "restart_successful", 1) : cJSON_AddNumberToObject(JSON_data, "restart_successful", 0); // approve -> 'restart'
    cJSON_AddNumberToObject(JSON_data, "IP_addr3", (STA_ADDR3) ? STA_ADDR3 : 0);                                                               // INTERNAL STORAGE
    char *string_json = cJSON_Print(JSON_data);
    ESP_LOGE("JSON_REPLY_RESTART", "%s", string_json);
    httpd_resp_set_type(req, "application/json"); // sending json data as response
    httpd_resp_sendstr(req, string_json);         // chunk
    httpd_resp_send(req, NULL, 0);
    free(string_json);
    cJSON_free(JSON_data);

    if (Esp_receive)
    {
        for (int i = 3; i > 0; i--)
        {
            ESP_LOGW("RESTART_TAG", "Restarting in ... %d", i);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        ESP_LOGE("RESTARTING ESP_32", "NOW...");
        vTaskDelay(pdMS_TO_TICKS(10));
        esp_restart();
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
    static auth_t cred;                           // cred obj // store it into the memory even after this function terminates
    char buffer[100];                             // this stores username and password into buffer //
    memset(&buffer, 0, sizeof(buffer));
    httpd_req_recv(req, buffer, req->content_len); // size_t recv_size = (req->content_len) > sizeof(buffer) ? sizeof(buffer) : (req->content_len);
    ESP_LOGW("login_data_post", "Buffer : %s", buffer);

    // get the json structured credentials
    cJSON *payload = cJSON_Parse(buffer); // returns an object holding respective [ key:value pair data ]
    strcpy(cred.username, cJSON_GetObjectItem(payload, "username")->valuestring);
    strcpy(cred.password, cJSON_GetObjectItem(payload, "password")->valuestring);
    ESP_LOGI("PARSE_TAG", "USERNAME = %s", cred.username);
    ESP_LOGI("PARSE_TAG", "PASSWORD = %s", cred.password);
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
    // sending json data
    cJSON *JSON_data = cJSON_CreateObject();
    cJSON_AddNumberToObject(JSON_data, "approve", (uint8_t)response.approve);
    char *string_json = cJSON_Print(JSON_data);
    ESP_LOGE("JSON_REPLY", "%s", string_json);
    httpd_resp_set_type(req, "application/json"); // sending json data as response
    httpd_resp_sendstr(req, string_json);         // chunk
    httpd_resp_send(req, NULL, 0);
    free(string_json);
    cJSON_free(JSON_data);
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
    if (strcmp(ext, ".jpg") == 0)
    {
        httpd_resp_set_type(req, "image/jpeg");
    }
    if (strcmp(ext, ".jpeg") == 0) // for image extensions
    {
        httpd_resp_set_type(req, "image/jpeg");
    }
    if (strcmp(ext, ".png") == 0) // for image extensions
    {
        httpd_resp_set_type(req, "image/png");
    }

    FILE *file = fopen(path, "rb"); // binary read // handle other files
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

    cJSON *payload = cJSON_Parse(buffer);
    strcpy(ap_config.local_ssid, cJSON_GetObjectItem(payload, "local_ssid")->valuestring); // this "ap_config" structure is passed to connect to new ssid
    strcpy(ap_config.local_pass, cJSON_GetObjectItem(payload, "local_pass")->valuestring);
    cJSON_Delete(payload);

    cJSON *JSON_data = cJSON_CreateObject();
    cJSON_AddNumberToObject(JSON_data, "IP_addr3", (STA_ADDR3) ? STA_ADDR3 : 0); // INTERNAL STORAGE
    char *string_json = cJSON_Print(JSON_data);
    ESP_LOGE("JSON_REPLY", "%s", string_json);
    httpd_resp_set_type(req, "application/json"); // sending json data as response
    httpd_resp_sendstr(req, string_json);         // chunk
    httpd_resp_send(req, NULL, 0);

    free(string_json);
    cJSON_free(JSON_data);

    vTaskDelay(200 / portTICK_PERIOD_MS);
    xTaskCreate(connect_to_local_AP, "connect_to_local_ap", 4096, &ap_config, 1, NULL);
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

// void start_MDNS(void)
// {
//     ESP_ERROR_CHECK(mdns_init());
//     ESP_ERROR_CHECK(mdns_hostname_set(local_server_name));
//     ESP_LOGI("MDNS_TAG", "mdns hostname set to: [%s]", local_server_name);
//     ESP_ERROR_CHECK(mdns_instance_name_set("local_Server_instance"));
//     mdns_txt_item_t serviceTxtData[3] = {
//         {"board", "esp32"},
//         {"u", "user"},
//         {"p", "password"}};
//     ESP_ERROR_CHECK(mdns_service_add("ESP32-WebServer", "_http", "_tcp", 80, serviceTxtData, 3));
//     ESP_LOGE("MDNS_TAG", "Server_name [STA-Mode] => '%s.local'", local_server_name);
// }
//
// static bool check_and_print_result(mdns_search_once_t *search)
// {
//     // Check if any result is available
//     mdns_result_t *result = NULL;
//     if (!mdns_query_async_get_results(search, 0, &result))
//     {
//         return false;
//     }
//     if (!result)
//     { // search timeout, but no result
//         return true;
//     }
//     // If yes, print the result
//     mdns_ip_addr_t *a = result->addr;
//     while (a)
//     {
//         if (a->addr.type == ESP_IPADDR_TYPE_V6)
//         {
//             printf("  AAAA: " IPV6STR "\n", IPV62STR(a->addr.u_addr.ip6));
//         }
//         else
//         {
//             printf("  A   : " IPSTR "\n", IP2STR(&(a->addr.u_addr.ip4)));
//         }
//         a = a->next;
//     }
//     // and free the result
//     mdns_query_results_free(result);
//     return true;
// }
// static void query_mdns_hosts_async(const char *host_name)
// {
//     ESP_LOGI("MDNS_TAG", "Query both A and AAAA: %s.local", host_name);
//     mdns_search_once_t *s_a = mdns_query_async_new(host_name, NULL, NULL, MDNS_TYPE_A, 1000, 1, NULL);
//     mdns_search_once_t *s_aaaa = mdns_query_async_new(host_name, NULL, NULL, MDNS_TYPE_AAAA, 1000, 1, NULL);
//     while (s_a || s_aaaa)
//     {
//         if (s_a && check_and_print_result(s_a))
//         {
//             ESP_LOGI("MDNS_TAG", "Query A %s.local finished", host_name);
//             mdns_query_async_delete(s_a);
//             s_a = NULL;
//         }
//         if (s_aaaa && check_and_print_result(s_aaaa))
//         {
//             ESP_LOGI("MDNS_TAG", "Query AAAA %s.local finished", host_name);
//             mdns_query_async_delete(s_aaaa);
//             s_aaaa = NULL;
//         }
//         vTaskDelay(50 / portTICK_PERIOD_MS);
//     }
// }

/*******************************************************************************
 *                          End of File
 *******************************************************************************/
