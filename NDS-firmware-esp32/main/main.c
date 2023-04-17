/*******************************************************************************
 * (C) Copyright 2018-2023 ;  Nepal Digital Systems Pvt. Ltd., Kathmandu, Nepal.
 * The attached material and the information contained therein is proprietary to
 * Nepal Digital Systems Pvt. Ltd. and is issued only under strict confidentiality
 * arrangements.It shall not be used, reproduced, copied in whole or in part,
 * adapted,modified, or disseminated without a written license of Nepal Digital
 * Systems Pvt. Ltd.It must be returned to Nepal Digital Systems Pvt. Ltd. upon
 * its first request.
 *
 *  File Name           : ESP32-Realy Controller
 *
 *  Description         : Relay control using ESP32.
 *
 *  Change history      :
 *
 *     Author        Date          Ver                 Description
 *  ------------    --------       ---   --------------------------------------
 *  Riken Maharjan  3 Mar 2023     1.0               Initial Creation
 *  Riken Maharjan  5 Mar 2023     1.1               Code Organiztion
 *
 *******************************************************************************/

/*******************************************************************************
 *                          Include Files
 *******************************************************************************/
#include <stdio.h>
#include "DATA_FIELD.h"
#include "server.h"
#include "connect.h"
#include "relay_pattern.h"
#include "reboot_count.h"
#include "restart_reset_random_intr.h"
// #include "nvs.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include "driver/timer.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_spiffs.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "esp_spi_flash.h"
#include "esp_system.h"
#include "lwip/sockets.h"
// #include "esp_intr_alloc.h"

/*******************************************************************************
 *                          Type & Macro Definitions
 *******************************************************************************/
/* LIST of namespaces and keys
 * namespace => sta_num ; key = no. ;
 * namespace => wifiCreds ; key = store_ssid  ;
 * namespace => wifiCreds ; key = store_pass  ;
 * namespace => loginCreds ; key = username  ;
 * namespace => loginCreds ; key = password  ;
 * namespace => Reboot ; key = statusAP  ;
 * namespace => Reboot ; key = statusSTA  ;
 * namespace => bootVal ; key = COUNT  ;
 * namespace => Relay_Status ; key = Relay1;
 * namespace => Relay_Status ; key = Relay2;
 * namespace => Relay_Status ; key = Relay3;
 * namespace => Relay_Status ; key = Relay4;
 * namespace => Relay_Status ; key = Relay5;
 * namespace => Relay_Status ; key = Relay6;
 * namespace => Relay_Status ; key = Relay7;
 * namespace => Relay_Status ; key = Relay8;
 * namespace => Relay_Status ; key = Relay9;
 * namespace => Relay_Status ; key = Relay10;
 * namespace => Relay_Status ; key = Relay11;
 * namespace => Relay_Status ; key = Relay12;
 * namespace => Relay_Status ; key = Relay13;
 * namespace => Relay_Status ; key = Relay14;
 * namespace => Relay_Status ; key = Relay15;
 * namespace => Relay_Status ; key = Relay16;
 * namespace => Relay_Status ; key = random;
 * namespace => Relay_Status ; key = serial;
 */

/* System LED ON - OFF */
#define SYS_LED 2
#define SYS_LED_OFF 0
#define SYS_LED_ON 1
/* Relay ON - OFF */
#define NUM_OF_RELAY 16
#define NUM_OF_LED_RELAY 12
#define R_ON 0
#define R_OFF 1

/* Reboot_Mode_index */
#define AP_mode 0
#define STA_mode 1

/* local_wifi cred index */
#define LOCAL_SSID_INDEX 0
#define LOCAL_PASS_INDEX 1

// Extern variables
extern esp_timer_handle_t esp_timer_handle1; // timer1 for indipendent serial pattern
extern esp_timer_handle_t esp_timer_handle2; // timer2 for indipendent random pattern

/*******************************************************************************
 *                          Static Function Definitions
 *******************************************************************************/

/**
 * @brief Function to inspect NVS_storage for Ssid_name/Ssid_pass and read them, if present.
 *
 * @param local_config A container to store the found data, Ssid_name and Ssid_pass
 * @param handle Storage Handle obtained from nvs_open function [to read/write]
 * @param KEY Name of the KEY corresponding to [Ssid_name | Ssid_pass]
 * @param index Index to determine what to select [Ssid_name => 0 | Ssid_pass => 1]
 *
 * @return - ESP_OK: wifi_creds data exists in nvs_storage.
 * @return - ESP_FAIL: wifi_creds data does not exist in nvs_storage.
 */
static esp_err_t inspect_wifiCred_data(ap_config_t *local_config, nvs_handle *handle, const char *KEY, uint8_t index)
{
    esp_err_t err = ESP_FAIL;
    size_t req_size;
    nvs_get_str(*handle, KEY, NULL, &req_size); // dynamic allocation
    char *sample = malloc(req_size);
    switch (nvs_get_str(*handle, KEY, sample, &req_size)) // test for ssid
    {
    case ESP_ERR_NVS_NOT_FOUND:
        ESP_LOGE("NVS_TAG", "%s value not set yet", KEY);
        break;
    case ESP_OK:
        if ((0 == index))
        {
            ESP_LOGI("NVS_TAG", "NVS_stored : Ssid is = %s", sample);
            strcpy(local_config->local_ssid, sample); // passing the extrated values
        }
        else if ((1 == index))
        {
            ESP_LOGI("NVS_TAG", "NVS_stored : Password is = %s", sample);
            strcpy(local_config->local_pass, sample);
        }
        err = ESP_OK; // indicating : [data exist]
        break;
    }
    ESP_ERROR_CHECK(nvs_commit(*handle));
    if (sample != NULL)
        free(sample);
    return err;
}

/**
 * @brief Function to initialize NVS_storage and retireve wifi credentials if present
 *
 * @param local_config A container to store the found data, Ssid_name and Ssid_pas
 *
 * @return - ESP_OK: nvs_storage initialized and data read complete.
 * @return - ESP_FAIL: nvs_storage not initialized or data not found.
 */
esp_err_t initialize_nvs(ap_config_t *local_config)
{
    // initialize the default NVS partition
    esp_err_t err = nvs_flash_init();
    if (ESP_ERR_NVS_NO_FREE_PAGES == err || ESP_ERR_NVS_NEW_VERSION_FOUND == err)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    nvs_handle handle;                                              // deta type -> nvs_handle_t ~ nvs_handle
    ESP_ERROR_CHECK(nvs_open("wifiCreds", NVS_READWRITE, &handle)); // namespace => loginCreds
    err = inspect_wifiCred_data(local_config, &handle, "store_ssid", LOCAL_SSID_INDEX);
    err = inspect_wifiCred_data(local_config, &handle, "store_pass", LOCAL_PASS_INDEX);
    nvs_close(handle);
    RESTART_WIFI(AP_mode); // inspect the AP_mode
    return (err);
}

/*******************************************************************************
 *                          Main Function
 *******************************************************************************/
void app_main(void)
{
    /***************************************************************/
    gpio_pad_select_gpio(SYS_LED);                 // SYS_LED turns ON in both AP & STA phase.
    gpio_set_direction(SYS_LED, GPIO_MODE_OUTPUT); // System ON/OFF-led initialized
    gpio_set_level(SYS_LED, SYS_LED_ON);           // turn-ON system led
    ESP_LOGE("SYSTEM_LED", "ON");                  // comment
    /***************************************************************/
    ap_config_t local_config; // ssid store sample
    // Create & initialize serial timer
    const esp_timer_create_args_t esp_timer_create_args1 = {
        .callback = Serial_Timer_Callback,
        .name = "Serial timer"};
    esp_timer_create(&esp_timer_create_args1, &esp_timer_handle1);
    /***************************************************************/
    // Create & initialize Random timer
    const esp_timer_create_args_t esp_timer_create_args2 = {
        .callback = Random_Timer_Callback,
        .name = "Random timer"};
    esp_timer_create(&esp_timer_create_args2, &esp_timer_handle2);
    /***************************************************************/
    wifi_init();               // wifi_initializtion
    INIT_RS_PIN();             // initialize the rest_activate
    INIT_RAND_PIN();           // initialize the random_activate
    setup_relay_update_task(); // create task to handle the relay activations
    if (ESP_OK == initialize_nvs(&local_config))
    {
        ESP_LOGE("LOCAL_SSID", "%s", local_config.local_ssid);
        ESP_LOGE("LOCAL_PASS", "%s", local_config.local_pass);
        if (ESP_OK == wifi_connect_sta(local_config.local_ssid, local_config.local_pass, 15000)) // if the local ssid/pass doesn't match
        {
            ESP_LOGW("STA_connect", "CONNECT TO LOCAL_SSID ... Successful.");
            RESTART_WIFI(STA_mode); // inspect the STA mode
            if (!get_STA_RESTART())
                esp_restart();
            Activate_Relays(); // activate the 'serial_operation_functionality'
        }
    }
    else
    {
        ESP_LOGW("AP_connect", "NO_LOCAL_SSID found.... Re-enter local network");
        wifi_connect_ap("ESP-32_local");
        if (!get_AP_RESTART())
            esp_restart();
        // Activate Serial and update in NVS_storage
        nvs_handle_t nvs_relay;
        nvs_open("Relay_Status", NVS_READWRITE, &nvs_relay);
        nvs_set_u8(nvs_relay, "random", 0); // random => [0/0ff] vs [1/ON , 2/ON , 3/ON , 4/ON]
        nvs_commit(nvs_relay);
        nvs_set_u8(nvs_relay, "serial", 1); // serial => [0/0ff] vs [1/ON]
        nvs_commit(nvs_relay);
        nvs_close(nvs_relay);
        Activate_Relays(); // activate the 'serial_operation_functionality'
    }
    Boot_count(); // increase the boot count
}

/*******************************************************************************
 *                          End of File
 *******************************************************************************/
