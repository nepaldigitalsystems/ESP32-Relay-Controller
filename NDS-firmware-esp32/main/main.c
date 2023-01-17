#include <stdio.h>
#include "DATA_FIELD.h"
#include "server.h"
#include "connect.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "toggleled.h"
#include "cJSON.h"

#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_spiffs.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_sntp.h"
#include "esp_spi_flash.h"
#include "esp_system.h"
#include "lwip/sockets.h"
#include "time.h"

/*
 * namespace => wifiCreds ; key = store_ssid  ;
 * namespace => wifiCreds ; key = store_pass  ;
 * namespace => loginCreds ; key = username  ;
 * namespace => loginCreds ; key = password  ;
 * namespace => Reboot ; key = statusAP  ;
 * namespace => Reboot ; key = statusSTA  ;
 * namespace => bootVal ; key = COUNT  ;
 */

/* Reboot mode index */
#define AP_mode 0
#define STA_mode 1
/* login cred index */
#define username_index 0
#define password_index 1
/* local_wifi cred index */
#define local_ssid_index 0
#define local_pass_index 1
/*Complie Date and Time (only once)*/
const char *compile_date = __DATE__;
const char *compile_time = __TIME__;

// have to send a struct inseade of void
bool AP_restart = false;  // Restart once
bool STA_restart = false; // Restart once

void RESTART_WIFI(uint8_t mode)
{
    // check if AP-reset is ['1' ~ true]
    nvs_handle reset;
    esp_err_t res = 0;
    ESP_ERROR_CHECK(nvs_open("Reboot", NVS_READWRITE, &reset));
    uint8_t status_val = 0;
    if (mode)
    {
        res = nvs_get_u8(reset, "statusSTA", &status_val);
        STA_restart = (bool)status_val;
    }
    else
    {
        res = nvs_get_u8(reset, "statusAP", &status_val);
        AP_restart = (bool)status_val;
    }

    switch (res)
    {
    case ESP_ERR_NVS_NOT_FOUND:
        ESP_LOGE("REBOOT_TAG", "Value not set yet");
        ESP_LOGE("REBOOT_TAG", "Restarting ...");
        break;
    case ESP_OK:
        if (mode)
            ESP_LOGE("REBOOT_TAG", "STA Restarted ? ... %s", (STA_restart ? "Already restarted once" : "not restarted"));
        else
            ESP_LOGE("REBOOT_TAG", "AP Restarted ? ... %s", (AP_restart ? "Already restarted once" : "not restarted"));
        break;
    default:
        break;
    }
    status_val = 1;

    if (mode) // STA_mode = 1
        ESP_ERROR_CHECK(nvs_set_u8(reset, "statusSTA", status_val));
    else
        ESP_ERROR_CHECK(nvs_set_u8(reset, "statusAP", status_val));

    ESP_ERROR_CHECK(nvs_commit(reset));
    nvs_close(reset);
}
esp_err_t inspect_login_data(auth_t *Data, nvs_handle *login, const char *KEY, uint8_t index)
{
    esp_err_t err = ESP_FAIL;
    size_t required_size;
    nvs_get_str(*login, KEY, NULL, &required_size); // dynamic allocation and required_size => length required to hold the value.
    char *sample = malloc(required_size);
    esp_err_t result = nvs_get_str(*login, KEY, sample, &required_size);
    switch (result)
    {
    case ESP_ERR_NVS_NOT_FOUND:
        ESP_LOGE("NVS_TAG", "%s not set yet.... Storing New & approved %s", KEY, KEY);
        err = ESP_OK; // writes to the local_config structure if [data doesn't exist]
        break;
    case ESP_OK:
        ESP_LOGI("NVS_TAG", "NVS_stored : %s is = %s", KEY, sample);
        if ((index == 0))
        {
            if (strcmp(Data->username, sample) == 0)
                ESP_LOGW("NVS_TAG", "USERNAME ... Approved");
        }
        else if ((index == 1))
        {
            if (strcmp(Data->password, sample) == 0)
                ESP_LOGW("NVS_TAG", "PASSWORD ... Approved");
        }
        break;
    default:
        ESP_LOGE("NVS_TAG", "Error (%s) opening NVS login!\n", esp_err_to_name(result));
        break;
    }
    ESP_ERROR_CHECK(nvs_commit(*login));
    if (sample != NULL)
        free(sample);
    return err;
}
esp_err_t inspect_wifiCred_data(ap_config_t *local_config, nvs_handle *handle, const char *KEY, uint8_t index)
{
    esp_err_t err = ESP_FAIL;
    size_t req_size;
    nvs_get_str(*handle, KEY, NULL, &req_size); // dynamic allocation
    char *sample = malloc(req_size);
    esp_err_t result = nvs_get_str(*handle, KEY, sample, &req_size); // key => store_ssid
    switch (result)                                                  // test for ssid
    {
    case ESP_ERR_NVS_NOT_FOUND:
        ESP_LOGE("NVS_TAG", "%s value not set yet", KEY);
        break;
    case ESP_OK:
        if ((index == 0))
        {
            ESP_LOGI("NVS_TAG", "NVS_stored : Ssid is = %s", sample);
            strcpy(local_config->local_ssid, sample); // passing the extrated values
        }
        else if ((index == 1))
        {
            ESP_LOGI("NVS_TAG", "NVS_stored : Password is = %s", sample);
            strcpy(local_config->local_pass, sample);
        }
        err = ESP_OK; // reads from the local_config structure if [data exist]
        break;
    default:
        ESP_LOGE("NVS_TAG", "Error (%s) opening NVS handle!\n", esp_err_to_name(result));
        break;
    }
    ESP_ERROR_CHECK(nvs_commit(*handle));
    if (sample != NULL)
        free(sample);
    return err;
}

esp_err_t login_cred(auth_t *Data) // compares the internal login creds with given "Data" arg
{
    nvs_handle login; // nvs_handle_t = nvs_handle
    esp_err_t err;
    ESP_ERROR_CHECK(nvs_open("loginCreds", NVS_READWRITE, &login)); // namespace => loginCreds
    err = inspect_login_data(Data, &login, "username", username_index);
    err = inspect_login_data(Data, &login, "password", password_index);
    nvs_close(login);
    return (err);
}

esp_err_t initialize_nvs(ap_config_t *local_config)
{
    // initialize the default NVS partition
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    nvs_handle handle;                                              // nvs_handle_t = nvs_handle
    ESP_ERROR_CHECK(nvs_open("wifiCreds", NVS_READWRITE, &handle)); // namespace => loginCreds
    err = inspect_wifiCred_data(local_config, &handle, "store_ssid", local_ssid_index);
    err = inspect_wifiCred_data(local_config, &handle, "store_pass", local_pass_index);
    nvs_close(handle);
    RESTART_WIFI(AP_mode);
    return (err);
}

void get_CompileTime(void)
{
    char timestamp[50];
    struct tm Start_Time;
    strptime(compile_date, "%b %d %Y", &Start_Time);
    strptime(compile_time, "%I:%M:%S", &Start_Time);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %I:%M:%S.%p", &Start_Time); // convert date and time to string
    ESP_LOGW("Compiled_Time", "  Start -----> %s", timestamp);
}

void Boot_count(void)
{
    nvs_handle BOOT;
    ESP_ERROR_CHECK(nvs_open("bootVal", NVS_READWRITE, &BOOT));

    uint16_t val = 0;
    esp_err_t result = nvs_get_u16(BOOT, "COUNT", &val);
    switch (result)
    {
    case ESP_ERR_NVS_NOT_FOUND:
    case ESP_ERR_NOT_FOUND:
        ESP_LOGW("BOOT_TAG", "First Boot.");
        break;
    case ESP_OK:
        ESP_LOGW("BOOT_TAG", "Boot_count => %d", val);
        break;
    default:
        ESP_LOGW("BOOT_TAG", "Error (%s) opening NVS BOOT_count!\n", esp_err_to_name(result));
        break;
    }
    val++;
    ESP_ERROR_CHECK(nvs_set_u16(BOOT, "COUNT", val));
    ESP_ERROR_CHECK(nvs_commit(BOOT));
    nvs_close(BOOT);
}

void CHIP_INFO(void)
{
    uint8_t chipId[6];
    esp_chip_info_t chip_info;
    int DRam = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    int IRam = heap_caps_get_free_size(MALLOC_CAP_32BIT) - heap_caps_get_free_size(MALLOC_CAP_8BIT);
    esp_efuse_mac_get_default(chipId);

    /*ESP32 info*/
    esp_chip_info(&chip_info);
    ESP_LOGI("CHIP_TAG", "This is ESP32 chip with %d CPU cores, WiFi%s%s, ",
             chip_info.cores,
             (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
             (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    ESP_LOGI("CHIP_TAG", "silicon revision %d, ", chip_info.revision);
    ESP_LOGI("CHIP_TAG", "%dMB %s flash", spi_flash_get_chip_size() / (1024 * 1024),
             (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
    ESP_LOGI("CHIP_TAG", "idf version is %s", esp_get_idf_version());
    ESP_LOGI("CHIP_TAG", "MAC/Chip id: %02x:%02x:%02x:%02x:%02x:%02x", chipId[0], chipId[1], chipId[2], chipId[3], chipId[4], chipId[5]);

    /*RAM info*/
    ESP_LOGI("CHIP_TAG", "Free Heap Size = %dkb [ DRAM ]", xPortGetFreeHeapSize());
    ESP_LOGI("CHIP_TAG", "DRAM = %d", DRam);
    ESP_LOGI("CHIP_TAG", "IRam = %d", IRam);
    int free = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    ESP_LOGI("CHIP_TAG", "free = %d", free);

    /*boot count*/
    Boot_count();
}

void app_main(void)
{
    ESP_LOGE("ESP_timer", "app started %lld mS", esp_timer_get_time() / 1000);
    get_CompileTime();

    ap_config_t local_config; // ssid store sample
    wifi_init();
    if (initialize_nvs(&local_config) == ESP_OK)
    {
        ESP_LOGE("LOCAL_SSID", "%s", local_config.local_ssid);
        ESP_LOGE("LOCAL_PASS", "%s", local_config.local_pass);
        if (wifi_connect_sta(local_config.local_ssid, local_config.local_pass, 15000) == ESP_OK) // if the local ssid/pass doesn't match
        {
            ESP_LOGW("STA_connect", "CONNECT TO LOCAL_SSID ... Successful.");
            RESTART_WIFI(STA_mode);
            if (!STA_restart)
            {
                esp_restart();
            }
        }
    }
    else
    {
        ESP_LOGW("AP_connect", "NO_LOCAL_SSID found.... Re-enter local network");
        wifi_connect_ap("ESP-32_local");
        if (!AP_restart)
        {
            esp_restart();
        }
    }
    // generate the chip info
    CHIP_INFO();
}
