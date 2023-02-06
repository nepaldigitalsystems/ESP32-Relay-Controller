#include <stdio.h>
#include "DATA_FIELD.h"
#include "server.h"
#include "connect.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "toggleled.h"
#include "cJSON.h"
#include "driver/timer.h"
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
#include "driver/gpio.h"
#include "mdns.h"

/* LIST of namespaces and keys
 * namespace => sta_num ; key = no. ;
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

/*Relay ON / OFF*/
#define r_ON 0
#define r_OFF 1
uint8_t Relay_Status_Value[RELAY_UPDATE_MAX] = {0};   // 1-18
uint8_t Relay_Update_Success[RELAY_UPDATE_MAX] = {0}; // 1-18

/* list of relay_pins [ 0-15 ] */
gpio_num_t relay_pins[16] = {GPIO_NUM_13, GPIO_NUM_12, GPIO_NUM_14, GPIO_NUM_27, GPIO_NUM_26, GPIO_NUM_25, GPIO_NUM_33, GPIO_NUM_32, GPIO_NUM_2, GPIO_NUM_4, GPIO_NUM_5, GPIO_NUM_18, GPIO_NUM_19, GPIO_NUM_21, GPIO_NUM_22, GPIO_NUM_23};
static TaskHandle_t receiveHandler = NULL;
xSemaphoreHandle xSema = NULL;
static esp_timer_handle_t esp_timer_handle1;
static esp_timer_handle_t esp_timer_handle2;

/* Reboot mode index */
#define AP_mode 0
#define STA_mode 1
/* login cred index */
#define username_index 0
#define password_index 1
/* local_wifi cred index */
#define local_ssid_index 0
#define local_pass_index 1

// have to send a struct inseade of void
static bool AP_restart = false;  // Restart once
static bool STA_restart = false; // Restart once
static bool pattern = 0;
static uint8_t combination = 0;

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
        ESP_LOGE("REBOOT_TAG", "Value not set yet . Restarting ...");
        break;
    case ESP_OK:
        if (mode)
            ESP_LOGE("REBOOT_TAG", "STA Restarted ? ... %s", (STA_restart ? "Already restarted once" : "not restarted"));
        else
            ESP_LOGE("REBOOT_TAG", "AP Restarted ? ... %s", (AP_restart ? "Already restarted once" : "not restarted"));
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
        err = ESP_OK; // reads from the local_config structure if [data exist]
        break;
    }
    ESP_ERROR_CHECK(nvs_commit(*handle));
    if (sample != NULL)
        free(sample);
    return err;
}

esp_err_t login_cred(auth_t *Data) // compares the internal login creds with given "Data" arg
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

void Boot_count(void)
{
    nvs_handle BOOT;
    ESP_ERROR_CHECK(nvs_open("bootVal", NVS_READWRITE, &BOOT));
    uint16_t val = 0;
    // esp_err_t result = nvs_get_u16(BOOT, "COUNT", &val);
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

static void Serial_Timer_Callback(void *params) // service routine for rtos-timer
{
    static uint8_t position = 0;
    if ((position > 1) && (position < 16))
    {
        gpio_set_level(relay_pins[position - 2], r_OFF);
        gpio_set_level(relay_pins[position - 1], r_ON);
        gpio_set_level(relay_pins[position], r_ON);
    }
    else if (position == 16)
        gpio_set_level(relay_pins[position - 2], r_OFF);
    else
    {
        gpio_set_level(relay_pins[position], r_ON);
        gpio_set_level(relay_pins[15], r_OFF);
    }
    // shift the pointer position
    position++;
    if (position > 16)
        position = 0;
}
static void Random_Timer_Callback(void *params) // service routine for rtos-timer
{
    static uint32_t random_seconds = 0;
    static uint32_t patt_arr[4][2][16] = {{{1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0}, {0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1}},
                                          {{1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0}, {0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1}},
                                          {{1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0}, {0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1}},
                                          {{1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1}}};

    // toggle the gpio according to [combination & pattern]
    pattern = ((random_seconds <= 3) || (random_seconds >= 8 && random_seconds <= 11)) ? 0 : 1; // the timer instants determine, which patther [A or B] is to be selected.
    for (uint8_t i = 0; i <= 15; i++)
    {
        gpio_set_level(relay_pins[i], (patt_arr[combination][pattern][i]));
    }

    random_seconds++;
    if (random_seconds > 15)
        random_seconds = 0;
}

static void Serial_Patttern_task(void *params) // receiver
{
    uint32_t state;
    static esp_err_t isOn = ESP_FAIL;
    while (1)
    {
        xTaskNotifyWait(0, 0, &state, portMAX_DELAY);
        ESP_LOGE("Notification_serial_task", "State_sent : %d", state);
        switch (state)
        {
        case 1:
            // Start the timer
            isOn = esp_timer_start_periodic(esp_timer_handle1, 1000000); // 1sec
            ESP_LOGI("SERIAL_TIMER", "STARTED : state - ON");
            break;
        case 0:
            if (isOn == ESP_OK)
            {
                esp_timer_stop(esp_timer_handle1);
                isOn = ESP_FAIL;
                ESP_LOGI("SERIAL_TIMER", "STOPPED : state - OFF");
            }
            break;
        }
    }
    vTaskDelete(NULL);
}

static void Random_Pattern_generator(uint8_t comb)
{
    static esp_err_t isOn = ESP_FAIL;
    if (comb != 0)
    {
        //     int lower = 0, upper = 15, arr_size = 0, count = 0, Asize = 0, temp = 0;
        //     int *append_num;
        //     time_t t;
        //     // decide the combinations pattern
        //     count = 16 - (int)comb;
        //     arr_size = sizeof(int) * count;
        //     append_num = (int *)malloc(arr_size);
        //     srand((unsigned)time(&t));
        //     // generate no of. 'count' => random values from [0-15]
        //     for (int i = 0; i < count; i++)
        //     {
        //         append_num[i] = (rand() % (upper - lower + 1)) + lower;
        //     }
        //     // remove duplicate from append_num if any
        //     Asize = arr_size / sizeof(int);
        //     for (int i = 0; i < (Asize - 1); i++)
        //     {
        //         for (int j = i + 1; j < Asize; j++)
        //         {
        //             if (*(append_num + i) == *(append_num + j))
        //             {
        //                 temp = *(append_num + j);
        //                 *(append_num + j) = *(append_num + Asize - 1);
        //                 *(append_num + Asize - 1) = temp;
        //                 Asize--;
        //             }
        //         }
        //     }
        //     // now apply relay ON/OFF
        //     for (int i = 0; i < Asize; i++)
        //     {
        //         // may need to invert the gpio_set_level
        //         Relay_Update_Success[append_num[i]] = (gpio_set_level(relay_pins[append_num[i]], (uint32_t)r_ON) == ESP_OK) ? 1 : 0; // set relay_update_success = '1', if the gpio_set is successful
        //     }

        // determine the combination
        combination = comb - 1;
        if (isOn == ESP_OK) // stop timer
        {
            esp_timer_stop(esp_timer_handle2);
            isOn = ESP_FAIL;
            ESP_LOGI("RANDOM_TIMER", "STOPPED : state - %d", comb);
        }
        isOn = esp_timer_start_periodic(esp_timer_handle2, 1000000); // 1sec // Start timer
        ESP_LOGI("RANDOM_TIMER", "STARTED : state - %d", comb);
    }
    else // 0 => stop timer
    {
        if (isOn == ESP_OK) // stop timer
        {
            esp_timer_stop(esp_timer_handle2);
            isOn = ESP_FAIL;
            ESP_LOGI("RANDOM_TIMER", "STOPPED : state - %d", comb);
        }
    }
}

static void Relay_switch_update(void *params) // sender
{
    for (int i = 0; i < 16; i++) // 0-15
    {
        gpio_pad_select_gpio(relay_pins[i]);
        gpio_set_direction(relay_pins[i], GPIO_MODE_OUTPUT);
    }
    while (1)
    {
        if (xSema != NULL)
        {
            // Gate to block from running , if access not given
            if (xSemaphoreTake(xSema, portMAX_DELAY))
            {
                /******************************************************************************************************************/
                // retrieve the Relay values from internal
                nvs_handle update;
                ESP_ERROR_CHECK(nvs_open("Relay_Status", NVS_READWRITE, &update));
                uint8_t val = 0;
                if (ESP_ERR_NVS_NOT_FOUND == nvs_get_u8(update, "random", &val))
                    Relay_Status_Value[RANDOM_UPDATE] = 0;
                else
                    Relay_Status_Value[RANDOM_UPDATE] = val; // 0,1,2,3,4
                // ESP_LOGW("DISPLAY_RELAY", "%s : stored_state -> %d : %s", "random", val, (Relay_Status_Value[RANDOM_UPDATE] ? "random_ON" : "random_OFF"));

                if (ESP_ERR_NVS_NOT_FOUND == nvs_get_u8(update, "serial", &val))
                    Relay_Status_Value[SERIAL_UPDATE] = 0;
                else
                    Relay_Status_Value[SERIAL_UPDATE] = val; // 1,0
                // ESP_LOGW("DISPLAY_RELAY", "%s : stored_state -> %d : %s", "serial", val, (Relay_Status_Value[SERIAL_UPDATE] ? "serial_ON" : "serial_OFF"));

                for (uint8_t i = 1; i <= RELAY_UPDATE_16; i++) // get "1/0" -> relay_status [1-16]
                {
                    char *str = (char *)malloc(sizeof("Relay") + 2);
                    memset(str, 0, sizeof("Relay") + 2);
                    sprintf(str, "Relay%u", i);
                    if (ESP_ERR_NVS_NOT_FOUND == nvs_get_u8(update, str, &val))
                        Relay_Status_Value[i] = r_OFF; // need to be inverted
                    else
                        Relay_Status_Value[i] = val; // 1,0
                    // ESP_LOGW("DISPLAY_RELAY", "%s : stored_state -> %s ", str, (Relay_Status_Value[i]) ? "r_OFF" : "r_ON");
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
                    for (int i = 0; i < 16; i++)
                    { // set relay_update_success = '1', if the gpio_set is successful // may need to invert the gpio_set_level
                        Relay_Update_Success[i + 1] = (gpio_set_level(relay_pins[i], (uint32_t)Relay_Status_Value[i + 1]) == ESP_OK) ? 1 : 0;
                    }
                    Relay_Update_Success[RANDOM_UPDATE] = 0;
                    Relay_Update_Success[SERIAL_UPDATE] = 0;
                }
                else
                {
                    memset(Relay_Update_Success, 0, sizeof(Relay_Update_Success)); // Except for 'Serial' ; set all reply status as false
                    for (int i = 0; i < 16; i++)                                   // clear out all the relays to activate serial or random phase
                    {
                        gpio_set_level(relay_pins[i], (uint32_t)r_OFF);
                    }
                    if (Relay_Status_Value[SERIAL_UPDATE] == 1 && Relay_Status_Value[RANDOM_UPDATE] == 0)
                    {
                        ESP_LOGI("Activate", " --> SERIAL PATTERN");
                        Relay_Update_Success[SERIAL_UPDATE] = 1;
                        // Random_Pattern_generator(0);
                        xTaskNotify(receiveHandler, (1 << 0), eSetValueWithOverwrite); // give 'activation notification' to serial task
                    }
                    else if (Relay_Status_Value[RANDOM_UPDATE] > 0 && Relay_Status_Value[SERIAL_UPDATE] == 0)
                    {
                        ESP_LOGI("Activate", " --> RANDOM PATTERN : %d", Relay_Status_Value[RANDOM_UPDATE]);
                        Relay_Update_Success[RANDOM_UPDATE] = 1;
                        // xTaskNotify(receiveHandler, (0 << 0), eSetValueWithOverwrite); // first check & deactivate serial timer
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

void app_main(void)
{
    /***************************************************************/
    // initialize serial timer
    const esp_timer_create_args_t esp_timer_create_args1 = {
        .callback = Serial_Timer_Callback,
        .name = "Serial timer"};
    esp_timer_create(&esp_timer_create_args1, &esp_timer_handle1);
    /***************************************************************/
    // initialize Random timer
    const esp_timer_create_args_t esp_timer_create_args2 = {
        .callback = Random_Timer_Callback,
        .name = "Random timer"};
    esp_timer_create(&esp_timer_create_args2, &esp_timer_handle2);
    /***************************************************************/

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
                esp_restart();
            xSema = xSemaphoreCreateBinary();
            xTaskCreate(Serial_Patttern_task, "Serial Pattern Generator", 4096, NULL, 5, &receiveHandler); // receiver
            xTaskCreate(Relay_switch_update, "Relay_switch_update", 4096, NULL, 6, NULL);                  // sender // higher priority
            xSemaphoreGive(xSema);                                                                         // activate the 'resume_operation_functionality'
        }
    }
    else
    {
        ESP_LOGW("AP_connect", "NO_LOCAL_SSID found.... Re-enter local network");
        wifi_connect_ap("ESP-32_local");
        if (!AP_restart)
            esp_restart();
    }
    Boot_count();
}
