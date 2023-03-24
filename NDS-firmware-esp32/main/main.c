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
#include "nvs.h"
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
#include "esp_intr_alloc.h"

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
/* random_pin, restart_reset and system_led pin declarations */
const uint32_t RS_PIN = 34;
const uint32_t RAND_PIN = 35;

/*******************************************************************************
 *                          Static Data Definitions
 *******************************************************************************/
/*Non-Static defination (externed)*/
xSemaphoreHandle xSEMA = NULL;                                                                                                                                            // global
uint8_t Relay_Status_Value[RELAY_UPDATE_MAX] = {0, R_OFF, R_OFF, R_OFF, R_OFF, R_OFF, R_OFF, R_OFF, R_OFF, R_OFF, R_OFF, R_OFF, R_OFF, R_OFF, R_OFF, R_OFF, R_OFF, 0, 0}; // 0-18
uint8_t Relay_Update_Success[RELAY_UPDATE_MAX] = {0};                                                                                                                     // 0-18

/*Static data defination*/
static gpio_num_t REALY_PINS[NUM_OF_RELAY] = {GPIO_NUM_13, GPIO_NUM_12, GPIO_NUM_14, GPIO_NUM_27, GPIO_NUM_26, GPIO_NUM_25, GPIO_NUM_33, GPIO_NUM_32, GPIO_NUM_15, GPIO_NUM_4, GPIO_NUM_5, GPIO_NUM_18, GPIO_NUM_19, GPIO_NUM_21, GPIO_NUM_22, GPIO_NUM_23};
static gpio_config_t io_conf;                // for multiple gpio configuration
static esp_timer_handle_t esp_timer_handle1; // timer1 for indipendent serial pattern
static esp_timer_handle_t esp_timer_handle2; // timer2 for indipendent random pattern
static TaskHandle_t receiveHandler = NULL;   // handler to notify the state of random_pattern currently being activated
static TaskHandle_t rsHandler = NULL;        // handler to notify, reset_restart interrupt button being pressed
static TaskHandle_t randHandler = NULL;      // handler to notify, random interrupt button being pressed
static bool AP_RESTART = false;              // a variable that remembers to -> Restart once in AP-mode at initialization stage
static bool STA_RESTART = false;             // a variable that remembers to -> Restart once in STA-mode at initialization stage
static bool PATTERN = 0;                     // a variable for random_state ; default = 0
static uint8_t COMBINATION = 0;              // a variable for random_state ; default = 0
static bool SERIAL_PENDING = false;          // variable to tackle a blocking condition, that occurs when random button [on the remote control], gets pressed during serial pattern operation.

/*******************************************************************************
 *                          Static Function Definitions
 *******************************************************************************/
/**
 * @brief Function to inspect nvs_storage for restart counts. if the system hasn't restarted once, this funtion generates a flag that will restart the system during initialization phase [begining of AP/STA]
 *
 * @param mode the index to determine current operating mode [AP => 0 | STA => 1]
 */
void RESTART_WIFI(uint8_t mode)
{
    nvs_handle reset;
    esp_err_t res = 0;
    uint8_t status_val = 0;
    ESP_ERROR_CHECK(nvs_open("Reboot", NVS_READWRITE, &reset));
    if (mode)
    {
        res = nvs_get_u8(reset, "statusSTA", &status_val); // get the internal value and store in "status_val"
        STA_RESTART = (bool)status_val;
    }
    else
    {
        res = nvs_get_u8(reset, "statusAP", &status_val); // get the internal value and store in "status_val"
        AP_RESTART = (bool)status_val;
    }
    switch (res)
    {
    case ESP_ERR_NVS_NOT_FOUND:
        ESP_LOGE("REBOOT_TAG", "Value not set yet . Restarting ...");
        break;
    case ESP_OK:
        if (mode)
            ESP_LOGE("REBOOT_TAG", "STA Restarted ? ... %s", (STA_RESTART ? "Already restarted once" : "not restarted"));
        else
            ESP_LOGE("REBOOT_TAG", "AP Restarted ? ... %s", (AP_RESTART ? "Already restarted once" : "not restarted"));
        break;
    }
    if (!status_val)
    {
        status_val = 1;
        if (mode)
            ESP_ERROR_CHECK(nvs_set_u8(reset, "statusSTA", status_val)); // set the internal value using "status_val"
        else
            ESP_ERROR_CHECK(nvs_set_u8(reset, "statusAP", status_val)); // set the internal value using "status_val"
    }
    ESP_ERROR_CHECK(nvs_commit(reset));
    nvs_close(reset);
}

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
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
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

/**
 * @brief Function to increase boot count after every system restart.
 *
 */
void Boot_count()
{
    nvs_handle BOOT;
    ESP_ERROR_CHECK(nvs_open("bootVal", NVS_READWRITE, &BOOT));
    uint16_t val = 0;
    if (ESP_ERR_NVS_NOT_FOUND == nvs_get_u16(BOOT, "COUNT", &val))
    {
        ESP_LOGW("BOOT_TAG", "First Boot.");
    }
    else
    {
        ESP_LOGW("BOOT_TAG", "Boot_count => %d", val);
    }
    val++;
    ESP_ERROR_CHECK(nvs_set_u16(BOOT, "COUNT", val));
    ESP_ERROR_CHECK(nvs_commit(BOOT));
    nvs_close(BOOT);
}

/**
 * @brief Secondary-Task that get activated as a callback for serial_timer within "serial pattern generator" task.
 *
 */
static void Serial_Timer_Callback(void *params) // Callback triggered every 1000mS -> 1s
{
    static int8_t position = 0;                            // relay position = 1
    static bool dir = 1;                                   // forward_dir = 1 // reverse_dir = 0;
    if (position < 0 || position > (NUM_OF_LED_RELAY - 1)) // position > 3) // determine direction
    {
        dir = !dir;
        if (position < 0)
        {
            position = 0;
            for (int i = 1; i < NUM_OF_LED_RELAY; i++) // clear out all LED-relays to activate serial or random phase
            {
                gpio_set_level(REALY_PINS[i], (uint32_t)R_OFF);
            }
        }
        else if (position > (NUM_OF_LED_RELAY - 1)) // position > 3)
        {
            position = (NUM_OF_LED_RELAY - 1);               // position = 3)
            for (int i = 0; i < (NUM_OF_LED_RELAY - 1); i++) // clear out all LED-relays to activate serial or random phase
            {
                gpio_set_level(REALY_PINS[i], (uint32_t)R_OFF);
            }
        }
    }
    else
    {
        gpio_set_level(REALY_PINS[position], R_ON); // [0 -> 3]
        // ESP_LOGW("Serial_led", "relay:%d [ %s ]", position, ((dir) ? "->" : "<-"));
    }
    (dir) ? position++ : position--; // increases the pointer position by 1 every second
}

/**
 * @brief Secondary-Task that get activated as a callback for random_timer within "random pattern generator" task.
 *
 */
static void Random_Timer_Callback(void *params) // Callback triggered every 1000mS -> 1s
{
    /* The "random_seconds" value ranges from [0-15] choices.
     * -> Currently, patterns [A/B] alternates at every interval of 4 calues.
     * i.e.[0-3]= A or 0
     * i.e.[4-7]= B or 1
     * i.e.[8-11]= A or 0
     * i.e.[12-15]= B or 1
     */
    static uint32_t random_seconds = 0;
    /* Here, patt_arr[][][] -> [random_combinations] ; [pattern_type(A/B)] ; [num_of_relay_components] */
    static uint32_t patt_arr[4][2][NUM_OF_LED_RELAY] = {{{1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0}, {0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1}},
                                                        {{1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0}, {0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1}},
                                                        {{1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1}, {0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0}},
                                                        {{1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1}}};

    // toggle the gpio according to [COMBINATION & PATTERN]
    PATTERN = ((random_seconds <= 3) || (random_seconds >= 8 && random_seconds <= 11)) ? 0 : 1; // the timer instants determine, which pattern [A or B] is to be selected.
    for (uint8_t i = 0; i < NUM_OF_LED_RELAY; i++)
    {
        gpio_set_level(REALY_PINS[i], (patt_arr[COMBINATION][PATTERN][i]));
    }
    /*Every 1 sec the random_seconds value increases by 1*/
    random_seconds++;
    if (random_seconds > 15)
        random_seconds = 0;
}

/**
 * @brief Primary-Task that get activated as a callback for serial pattern generator.
 *
 */
static void Serial_Patttern_task(void *params) // receiver
{
    static uint32_t state;
    static esp_err_t isOn1 = ESP_FAIL;
    while (1)
    {
        xTaskNotifyWait(0, 0, &state, portMAX_DELAY);
        switch (state)
        {
        case 1: // 1 => Start the timer
            isOn1 = esp_timer_start_periodic(esp_timer_handle1, 1000000);
            break;
        case 0: // 0 => stop timer
            if (isOn1 == ESP_OK)
            {
                esp_timer_stop(esp_timer_handle1);
                isOn1 = ESP_FAIL;
            }
            break;
        }
    }
    vTaskDelete(NULL);
}

/**
 * @brief Primary-Task that get activated as a callback for random pattern generator.
 *
 * @param comb 1-Byte value indicating current combination choice for random_pattern
 */
static void Random_Pattern_generator(uint8_t comb)
{
    static esp_err_t isOn2 = ESP_FAIL;
    if (comb != 0)
    {
        COMBINATION = comb - 1; // determine the COMBINATION {0,1,2,3,4}
        if (isOn2 == ESP_OK)    // stop timer
        {
            esp_timer_stop(esp_timer_handle2);
            isOn2 = ESP_FAIL;
        }
        isOn2 = esp_timer_start_periodic(esp_timer_handle2, 1000000); // Start timer
    }
    else // 0 => stop timer
    {
        if (isOn2 == ESP_OK) // stop timer
        {
            esp_timer_stop(esp_timer_handle2);
            isOn2 = ESP_FAIL;
        }
    }
}

/**
 * @brief Task to update relay_switch_states according to internally stored values.
 *
 */
static void Relay_switch_update(void *params) // -> also a notification sender task
{
    // all relay state -> OFF
    for (int i = 0; i < NUM_OF_RELAY; i++) // 0-15
    {
        gpio_pad_select_gpio(REALY_PINS[i]);
        gpio_set_direction(REALY_PINS[i], GPIO_MODE_OUTPUT);
        gpio_set_level(REALY_PINS[i], (uint32_t)R_OFF);
    }
    while (1)
    {
        if (xSEMA != NULL)
        {
            if (xSemaphoreTake(xSEMA, portMAX_DELAY)) // task blocked from running , if access not given
            {
                /******************************************************************************************************************/
                // retrieve the Relay values from internal
                nvs_handle update;
                ESP_ERROR_CHECK(nvs_open("Relay_Status", NVS_READWRITE, &update));
                uint8_t val = 0;
                if (ESP_ERR_NVS_NOT_FOUND == nvs_get_u8(update, "random", &val))
                    Relay_Status_Value[RANDOM_UPDATE] = 0; // random status value must be ->0 (default)
                else
                    Relay_Status_Value[RANDOM_UPDATE] = val; // 0,1,2,3,4
                // ESP_LOGW("DISPLAY_RELAY", "%s : stored_state -> %d : %s", "random", val, (Relay_Status_Value[RANDOM_UPDATE] ? "random_ON" : "random_OFF"));

                if (ESP_ERR_NVS_NOT_FOUND == nvs_get_u8(update, "serial", &val))
                    Relay_Status_Value[SERIAL_UPDATE] = 0; // serial status value must be ->0 (default)
                else
                    Relay_Status_Value[SERIAL_UPDATE] = val; // 1,0
                // ESP_LOGW("DISPLAY_RELAY", "%s : stored_state -> %d : %s", "serial", val, (Relay_Status_Value[SERIAL_UPDATE] ? "serial_ON" : "serial_OFF"));

                for (uint8_t i = 1; i <= RELAY_UPDATE_16; i++) // get "1/0" -> relay_status [1-16]
                {
                    char *str = (char *)malloc(sizeof("Relay") + 2);
                    memset(str, 0, sizeof("Relay") + 2);
                    sprintf(str, "Relay%u", i);
                    if (ESP_ERR_NVS_NOT_FOUND == nvs_get_u8(update, str, &val))
                        Relay_Status_Value[i] = R_OFF; // need to be inverted
                    else
                        Relay_Status_Value[i] = val; // 1,0
                    // ESP_LOGW("DISPLAY_RELAY", "%s : stored_state -> %s ", str, (Relay_Status_Value[i]) ? "R_OFF" : "R_ON");
                    free(str);
                }
                nvs_close(update); // no commits to avoid changes

                /****************************************************************************************************************************/
                // Generate the "update_success_status" for each button
                if (Relay_Status_Value[SERIAL_UPDATE] == 0 && Relay_Status_Value[RANDOM_UPDATE] == 0)
                {
                    ESP_LOGI("Activate", " --> BTNS ONLY");
                    xTaskNotify(receiveHandler, (0 << 0), eSetValueWithOverwrite);
                    Random_Pattern_generator(0);
                    for (int i = 0; i < NUM_OF_RELAY; i++)
                    { // set relay_update_success = '1', if the gpio_set is successful // may need to invert the gpio_set_level
                        Relay_Update_Success[i + 1] = (gpio_set_level(REALY_PINS[i], (uint32_t)Relay_Status_Value[i + 1]) == ESP_OK) ? 1 : 0;
                    }
                    Relay_Update_Success[RANDOM_UPDATE] = 0;
                    Relay_Update_Success[SERIAL_UPDATE] = 0;
                }
                else
                {
                    memset(Relay_Update_Success, 0, sizeof(Relay_Update_Success)); // Except for 'Serial' ; set all reply status as false
                    for (int i = 0; i < NUM_OF_LED_RELAY; i++)                     // clear out all LED-relays to activate serial or random phase
                    {
                        gpio_set_level(REALY_PINS[i], (uint32_t)R_OFF);
                    }
                    if (Relay_Status_Value[SERIAL_UPDATE] == 1 && Relay_Status_Value[RANDOM_UPDATE] == 0)
                    {
                        Random_Pattern_generator(0); // turn off random
                        ESP_LOGI("Activate", " --> SERIAL PATTERN");
                        Relay_Update_Success[SERIAL_UPDATE] = 1;
                        xTaskNotify(receiveHandler, (1 << 0), eSetValueWithOverwrite); // give 'activation notification' to serial task
                    }
                    else if (Relay_Status_Value[RANDOM_UPDATE] > 0 && Relay_Status_Value[SERIAL_UPDATE] == 0)
                    {
                        xTaskNotify(receiveHandler, (0 << 0), eSetValueWithOverwrite); // turn off serial
                        ESP_LOGI("Activate", " --> RANDOM PATTERN : %d", Relay_Status_Value[RANDOM_UPDATE]);
                        Relay_Update_Success[RANDOM_UPDATE] = 1;
                        Random_Pattern_generator(Relay_Status_Value[RANDOM_UPDATE]); // call a function to generate random patterns
                    }
                    else
                    {
                        xTaskNotify(receiveHandler, (0 << 0), eSetValueWithOverwrite); // first check & deactivate serial timer
                        Random_Pattern_generator(0);                                   // call a function to generate random patterns
                        ESP_LOGE("REALY_STATE_ERROR_NVS", "!! RANDOM [%d] and SERIAL [%d] button are in invalid state !!", Relay_Status_Value[RANDOM_UPDATE], Relay_Status_Value[SERIAL_UPDATE]);
                    }
                }
                /******************************************************************************************************************/
            }
        }
    }
    vTaskDelete(NULL);
}

/**
 * @brief Task to update relay_switch_states according to internally stored values.
 *
 * @param args GPIO_Pin_number passed to invoke corresponding action
 */
static void IRAM_ATTR restart_reset_random_isr(void *args)
{
    uint32_t *pin = (uint32_t *)args;
    switch (*pin)
    {
    case RS_PIN:
        xTaskNotifyGive(rsHandler); // activate RESET/RESTART action
        break;
    case RAND_PIN:
        xTaskNotifyGive(randHandler); // activate RANDOM action
        break;
    }
}

/**
 * @brief Task for restart_reset_button operation.
 *
 */
void restart_reset_Task(void *params)
{
    while (1)
    {
        if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY))
        {
            gpio_isr_handler_remove(RS_PIN);                       // disable the interrupt
            unsigned long millis_up = esp_timer_get_time() / 1000; // get the time at [positive edge triggered instant]
            ESP_LOGE("SYSTEM_LED", "OFF");
            gpio_set_level(SYS_LED, SYS_LED_OFF); // turn-OFF system led
            do
            {
                vTaskDelay(400 / portTICK_PERIOD_MS); // wait some time while we check for the button to be released
            } while (gpio_get_level(RS_PIN) == 1);
            unsigned long millis_down = esp_timer_get_time() / 1000; // get the time at [ logic low ]
            if ((int)(millis_down - millis_up) > 2000)
            {
                if ((int)(millis_down - millis_up) >= 5000)
                {
                    ESP_LOGE("RESTART_RESET_TAG", "GPIO '%d' was pressed for %d mSec. Reseting....", RS_PIN, (int)(millis_down - millis_up));
                    nvs_handle_t my_handle;
                    ESP_ERROR_CHECK(nvs_open("wifiCreds", NVS_READWRITE, &my_handle));
                    ESP_ERROR_CHECK(nvs_erase_all(my_handle));
                    ESP_ERROR_CHECK(nvs_commit(my_handle));
                    ESP_ERROR_CHECK(nvs_open("loginCreds", NVS_READWRITE, &my_handle));
                    ESP_ERROR_CHECK(nvs_erase_all(my_handle));
                    ESP_ERROR_CHECK(nvs_commit(my_handle));
                    ESP_ERROR_CHECK(nvs_open("sta_num", NVS_READWRITE, &my_handle));
                    ESP_ERROR_CHECK(nvs_erase_all(my_handle));
                    ESP_ERROR_CHECK(nvs_commit(my_handle));
                    ESP_ERROR_CHECK(nvs_open("Reboot", NVS_READWRITE, &my_handle));
                    ESP_ERROR_CHECK(nvs_erase_all(my_handle));
                    ESP_ERROR_CHECK(nvs_commit(my_handle));
                    ESP_ERROR_CHECK(nvs_open("bootVal", NVS_READWRITE, &my_handle));
                    ESP_ERROR_CHECK(nvs_erase_all(my_handle));
                    ESP_ERROR_CHECK(nvs_commit(my_handle));
                    ESP_ERROR_CHECK(nvs_open("Relay_Status", NVS_READWRITE, &my_handle));
                    ESP_ERROR_CHECK(nvs_erase_all(my_handle));
                    ESP_ERROR_CHECK(nvs_commit(my_handle));
                    nvs_close(my_handle);
                    for (int i = 0; i < NUM_OF_RELAY; i++) // clear out all the relays to activate serial or random phase
                    {
                        gpio_set_level(REALY_PINS[i], (uint32_t)R_OFF);
                        Relay_Status_Value[i + 1] = R_OFF;
                    }
                    ESP_LOGE("SYSTEM_LED", "ON");
                    gpio_set_level(SYS_LED, SYS_LED_ON);                                     // turn-ON system led
                    gpio_isr_handler_add(RS_PIN, restart_reset_random_isr, (void *)&RS_PIN); // re-enable the interrupt
                    esp_restart();
                }
                else
                {
                    ESP_LOGE("RESTART_RESET_TAG", "GPIO '%d' was pressed for %d mSec. Restarting....", RS_PIN, (int)(millis_down - millis_up));
                    ESP_LOGE("SYSTEM_LED", "ON");
                    gpio_set_level(SYS_LED, SYS_LED_ON);                                     // turn-ON system led
                    gpio_isr_handler_add(RS_PIN, restart_reset_random_isr, (void *)&RS_PIN); // re-enable the interrupt
                    esp_restart();
                }
            }
            ESP_LOGE("SYSTEM_LED", "ON");
            gpio_set_level(SYS_LED, SYS_LED_ON);                                     // turn-ON system led
            gpio_isr_handler_add(RS_PIN, restart_reset_random_isr, (void *)&RS_PIN); // re-enable the interrupt
        }
    }
    vTaskDelete(NULL);
}

/**
 * @brief Task for random_button operation.
 *
 */
void random_activate_Task(void *params)
{
    while (1)
    {
        if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY))
        {
            gpio_isr_handler_remove(RAND_PIN);                     // disable the interrupt
            unsigned long millis_up = esp_timer_get_time() / 1000; // get the time at [positive edge triggered instant]
            ESP_LOGE("SYSTEM_LED", "OFF");
            gpio_set_level(SYS_LED, SYS_LED_OFF); // turn-OFF system led
            do
            {
                vTaskDelay(50 / portTICK_PERIOD_MS); // wait some time while we check for the button to be released
            } while (gpio_get_level(RAND_PIN) == 1);
            unsigned long millis_down = esp_timer_get_time() / 1000; // get the time at [ logic low ]

            if ((int)(millis_down - millis_up) > 200) // ms
            {
                // first check if condition matches [ random => 'off' and serial => 'ON' ]
                if ((Relay_Status_Value[SERIAL_UPDATE] != 0) && (Relay_Status_Value[RANDOM_UPDATE] == 0))
                {
                    SERIAL_PENDING = true; // this gets activated only once ; [ie when serial=1 and random is activated using interrupt]
                }

                Relay_Status_Value[SERIAL_UPDATE] = 0; // clearing the serial status to avoid both being activated
                if (Relay_Status_Value[RANDOM_UPDATE] < 4)
                {
                    Relay_Status_Value[RANDOM_UPDATE] += 1;
                }
                else
                {
                    Relay_Status_Value[RANDOM_UPDATE] = 0;
                }
                nvs_handle_t nvs_relay;
                nvs_open("Relay_Status", NVS_READWRITE, &nvs_relay);
                nvs_set_u8(nvs_relay, "random", Relay_Status_Value[RANDOM_UPDATE]); // random => [0/0ff] vs [1/ON , 2/ON , 3/ON , 4/ON]
                nvs_commit(nvs_relay);
                if ((SERIAL_PENDING) && (Relay_Status_Value[RANDOM_UPDATE] == 0))
                {
                    SERIAL_PENDING = false;
                    nvs_set_u8(nvs_relay, "serial", 1);
                    nvs_commit(nvs_relay);
                }
                else
                {
                    nvs_set_u8(nvs_relay, "serial", 0); // serial => [0/0ff] vs [1/ON]
                    nvs_commit(nvs_relay);
                }
                nvs_close(nvs_relay);
                xSemaphoreGive(xSEMA);
            }
            ESP_LOGE("SYSTEM_LED", "ON");
            gpio_set_level(SYS_LED, SYS_LED_ON);                                         // turn-ON system led
            gpio_isr_handler_add(RAND_PIN, restart_reset_random_isr, (void *)&RAND_PIN); // re-enable the interrupt
        }
    }
    vTaskDelete(NULL);
}

/**
 * @brief Function to initailized restart_reset interrupt pin.
 *
 */
static void INIT_RS_PIN()
{
    io_conf.intr_type = GPIO_INTR_POSEDGE; // initialize the interrupt button for restart and reset
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_down_en = true;
    io_conf.pull_up_en = false;
    io_conf.pin_bit_mask = (1ULL << RS_PIN);
    gpio_config(&io_conf);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(RS_PIN, restart_reset_random_isr, (void *)&RS_PIN);          // activate the isr for RS_pin
    xTaskCreate(restart_reset_Task, "restart_reset_Task", 4096, NULL, 1, &rsHandler); // init restart_reset_task
}

/**
 * @brief Function to initailized random interrupt pin.
 */
static void INIT_RAND_PIN()
{
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_down_en = true;
    io_conf.pull_up_en = false;
    io_conf.pin_bit_mask = (1ULL << RAND_PIN);
    gpio_config(&io_conf);
    gpio_isr_handler_add(RAND_PIN, restart_reset_random_isr, (void *)&RAND_PIN);            // activate the isr for RS_pin
    xTaskCreate(random_activate_Task, "random_activate_Task", 4096, NULL, 1, &randHandler); // init random_activate_task
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
    wifi_init();                                                                                   // wifi_initializtion
    INIT_RS_PIN();                                                                                 // initialize the rest_activate
    INIT_RAND_PIN();                                                                               // initialize the random_activate
    xSEMA = xSemaphoreCreateBinary();                                                              // semaphore for Relay_switch_update task
    xTaskCreate(Serial_Patttern_task, "Serial Pattern Generator", 4096, NULL, 5, &receiveHandler); // receiver
    xTaskCreate(Relay_switch_update, "Relay_switch_update", 4096, NULL, 6, NULL);                  // sender // higher priority
    if (initialize_nvs(&local_config) == ESP_OK)
    {
        ESP_LOGE("LOCAL_SSID", "%s", local_config.local_ssid);
        ESP_LOGE("LOCAL_PASS", "%s", local_config.local_pass);
        if (wifi_connect_sta(local_config.local_ssid, local_config.local_pass, 15000) == ESP_OK) // if the local ssid/pass doesn't match
        {
            ESP_LOGW("STA_connect", "CONNECT TO LOCAL_SSID ... Successful.");
            RESTART_WIFI(STA_mode); // inspect the STA mode
            if (!STA_RESTART)
                esp_restart();
            xSemaphoreGive(xSEMA); // activate the 'serial_operation_functionality'
        }
    }
    else
    {
        ESP_LOGW("AP_connect", "NO_LOCAL_SSID found.... Re-enter local network");
        wifi_connect_ap("ESP-32_local");
        if (!AP_RESTART)
            esp_restart();
        // Activate Serial and update in NVS_storage
        nvs_handle_t nvs_relay;
        nvs_open("Relay_Status", NVS_READWRITE, &nvs_relay);
        nvs_set_u8(nvs_relay, "random", 0); // random => [0/0ff] vs [1/ON , 2/ON , 3/ON , 4/ON]
        nvs_commit(nvs_relay);
        nvs_set_u8(nvs_relay, "serial", 1); // serial => [0/0ff] vs [1/ON]
        nvs_commit(nvs_relay);
        nvs_close(nvs_relay);
        xSemaphoreGive(xSEMA); // activate the 'serial_operation_functionality'
    }
    Boot_count(); // increase the boot count
}
/*******************************************************************************
 *                          End of File
 *******************************************************************************/
