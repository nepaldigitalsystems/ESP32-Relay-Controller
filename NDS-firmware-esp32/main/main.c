#include <stdio.h>
#include "DATA_FIELD.h"
#include "server.h"
#include "connect.h"
#include "nvs.h"
#include "nvs_flash.h"
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
#include "esp_intr_alloc.h"

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
/*random_pin, restart_reset and system_led pin declarations*/
const uint32_t RS_PIN = 34;
const uint32_t RAND_PIN = 35;
#define SYS_LED 2
/*Relay ON - OFF*/
#define R_ON 0
#define R_OFF 1
/* Reboot mode index */
#define AP_mode 0
#define STA_mode 1
/* local_wifi cred index */
#define LOCAL_SSID_INDEX 0
#define LOCAL_PASS_INDEX 1

xSemaphoreHandle xSEMA = NULL;                                                                                                                                            // global
uint8_t Relay_Status_Value[RELAY_UPDATE_MAX] = {0, R_OFF, R_OFF, R_OFF, R_OFF, R_OFF, R_OFF, R_OFF, R_OFF, R_OFF, R_OFF, R_OFF, R_OFF, R_OFF, R_OFF, R_OFF, R_OFF, 0, 0}; // 0-18
uint8_t Relay_Update_Success[RELAY_UPDATE_MAX] = {0};                                                                                                                     // 0-18

/* list of REALY_PINS [ 0-15 ] */
static gpio_num_t REALY_PINS[16] = {GPIO_NUM_13, GPIO_NUM_12, GPIO_NUM_14, GPIO_NUM_27, GPIO_NUM_26, GPIO_NUM_25, GPIO_NUM_33, GPIO_NUM_32, GPIO_NUM_15, GPIO_NUM_4, GPIO_NUM_5, GPIO_NUM_18, GPIO_NUM_19, GPIO_NUM_21, GPIO_NUM_22, GPIO_NUM_23};
static TaskHandle_t receiveHandler = NULL;
static TaskHandle_t rsHandler = NULL;
static TaskHandle_t randHandler = NULL;
static esp_timer_handle_t esp_timer_handle1;
static esp_timer_handle_t esp_timer_handle2;
static bool AP_RESTART = false;  // a variable that remembers to -> Restart once in AP-mode at initialization stage
static bool STA_RESTART = false; // a variable that remembers to -> Restart once in STA-mode at initialization stage
static bool PATTERN = 0;         // a variable for random_state
static uint8_t COMBINATION = 0;  // a variable for random_state
static gpio_config_t io_conf;

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
        STA_RESTART = (bool)status_val;
    }
    else
    {
        res = nvs_get_u8(reset, "statusAP", &status_val);
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
    status_val = 1;

    if (mode) // STA_mode = 1
        ESP_ERROR_CHECK(nvs_set_u8(reset, "statusSTA", status_val));
    else
        ESP_ERROR_CHECK(nvs_set_u8(reset, "statusAP", status_val));

    ESP_ERROR_CHECK(nvs_commit(reset));
    nvs_close(reset);
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
    err = inspect_wifiCred_data(local_config, &handle, "store_ssid", LOCAL_SSID_INDEX);
    err = inspect_wifiCred_data(local_config, &handle, "store_pass", LOCAL_PASS_INDEX);
    nvs_close(handle);
    RESTART_WIFI(AP_mode);
    return (err);
}
void Boot_count(void)
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

static void Serial_Timer_Callback(void *params) // service routine for rtos-timer
{
    static uint8_t position = 0;
    if ((position > 1) && (position < 16))
    {
        gpio_set_level(REALY_PINS[position - 2], R_OFF);
        gpio_set_level(REALY_PINS[position - 1], R_ON);
        gpio_set_level(REALY_PINS[position], R_ON);
    }
    else if (position == 16)
        gpio_set_level(REALY_PINS[position - 2], R_OFF);
    else
    {
        gpio_set_level(REALY_PINS[position], R_ON);
        gpio_set_level(REALY_PINS[15], R_OFF);
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

    // toggle the gpio according to [COMBINATION & PATTERN]
    PATTERN = ((random_seconds <= 3) || (random_seconds >= 8 && random_seconds <= 11)) ? 0 : 1; // the timer instants determine, which patther [A or B] is to be selected.
    for (uint8_t i = 0; i <= 15; i++)
    {
        gpio_set_level(REALY_PINS[i], (patt_arr[COMBINATION][PATTERN][i]));
    }

    random_seconds++;
    if (random_seconds > 15)
        random_seconds = 0;
}
static void Serial_Patttern_task(void *params) // receiver
{
    uint32_t state;
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
static void Random_Pattern_generator(uint8_t comb)
{
    static esp_err_t isOn2 = ESP_FAIL;
    if (comb != 0)
    {
        /*
        //     int lower = 0, upper = 15, arr_size = 0, count = 0, Asize = 0, temp = 0;
        //     int *append_num;
        //     time_t t;
        //     // decide the combinations PATTERN
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
        //         Relay_Update_Success[append_num[i]] = (gpio_set_level(REALY_PINS[append_num[i]], (uint32_t)R_ON) == ESP_OK) ? 1 : 0; // set relay_update_success = '1', if the gpio_set is successful
        //     }
        */
        // determine the COMBINATION {0,1,2,3,4}
        COMBINATION = comb - 1;
        if (isOn2 == ESP_OK) // stop timer
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
static void Relay_switch_update(void *params) // -> also a notification sender task
{
    for (int i = 0; i < 16; i++) // 0-15
    {
        gpio_pad_select_gpio(REALY_PINS[i]);
        gpio_set_direction(REALY_PINS[i], GPIO_MODE_OUTPUT);
        gpio_set_level(REALY_PINS[i], (uint32_t)R_OFF);
    }

    while (true)
    {
        if (xSEMA != NULL)
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
                    for (int i = 0; i < 16; i++)
                    { // set relay_update_success = '1', if the gpio_set is successful // may need to invert the gpio_set_level
                        Relay_Update_Success[i + 1] = (gpio_set_level(REALY_PINS[i], (uint32_t)Relay_Status_Value[i + 1]) == ESP_OK) ? 1 : 0;
                    }
                    Relay_Update_Success[RANDOM_UPDATE] = 0;
                    Relay_Update_Success[SERIAL_UPDATE] = 0;
                }
                else
                {
                    memset(Relay_Update_Success, 0, sizeof(Relay_Update_Success)); // Except for 'Serial' ; set all reply status as false
                    for (int i = 0; i < 16; i++)                                   // clear out all the relays to activate serial or random phase
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
    vTaskDelete(NULL);
}

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

void restart_reset_Task(void *params)
{
    while (1)
    {
        if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY))
        {
            gpio_isr_handler_remove(RS_PIN);                       // disable the interrupt
            unsigned long millis_up = esp_timer_get_time() / 1000; // get the time at [positive edge triggered instant]
            ESP_LOGE("SYSTEM_LED", "OFF");
            gpio_set_level(SYS_LED, 0); // turn-OFF system led
            do
            {
                vTaskDelay(100 / portTICK_PERIOD_MS); // wait some time while we check for the button to be released
            } while (gpio_get_level(RS_PIN) == 1);
            unsigned long millis_down = esp_timer_get_time() / 1000; // get the time at [ logic low ]
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
                for (int i = 0; i < 16; i++) // clear out all the relays to activate serial or random phase
                {
                    gpio_set_level(REALY_PINS[i], (uint32_t)R_OFF);
                    Relay_Status_Value[i + 1] = R_OFF;
                }
            }
            else
            {
                ESP_LOGE("RESTART_RESET_TAG", "GPIO '%d' was pressed for %d mSec. Restarting....", RS_PIN, (int)(millis_down - millis_up));
            }
            ESP_LOGE("SYSTEM_LED", "ON");
            gpio_set_level(SYS_LED, 1);                                              // turn-ON system led
            gpio_isr_handler_add(RS_PIN, restart_reset_random_isr, (void *)&RS_PIN); // re-enable the interrupt
            esp_restart();
        }
    }
    vTaskDelete(NULL);
}

void random_activate_Task(void *params)
{
    while (true)
    {
        if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY))
        {
            gpio_isr_handler_remove(RAND_PIN);                     // disable the interrupt
            unsigned long millis_up = esp_timer_get_time() / 1000; // get the time at [positive edge triggered instant]
            ESP_LOGE("SYSTEM_LED", "OFF");
            gpio_set_level(SYS_LED, 0); // turn-OFF system led
            do
            {
                vTaskDelay(50 / portTICK_PERIOD_MS); // wait some time while we check for the button to be released
            } while (gpio_get_level(RAND_PIN) == 1);
            unsigned long millis_down = esp_timer_get_time() / 1000; // get the time at [ logic low ]
            if ((int)(millis_down - millis_up) >= 150)
            {
                Relay_Status_Value[SERIAL_UPDATE] = 0;
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
                nvs_set_u8(nvs_relay, "serial", 0); // serial => [0/0ff] vs [1/ON]
                nvs_commit(nvs_relay);
                nvs_close(nvs_relay);
                xSemaphoreGive(xSEMA);
            }
            ESP_LOGE("SYSTEM_LED", "ON");
            gpio_set_level(SYS_LED, 1);                                                  // turn-ON system led
            gpio_isr_handler_add(RAND_PIN, restart_reset_random_isr, (void *)&RAND_PIN); // re-enable the interrupt
        }
    }
    vTaskDelete(NULL);
}

static void INIT_RS_PIN(void)
{
    gpio_pad_select_gpio(SYS_LED);                 // SYS_LED turns ON in both AP & STA phase.
    gpio_set_direction(SYS_LED, GPIO_MODE_OUTPUT); // System ON/OFF-led initialized
    gpio_set_level(SYS_LED, 1);                    // turn-ON system led
    ESP_LOGE("SYSTEM_LED", "ON");

    io_conf.intr_type = GPIO_INTR_POSEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_down_en = true;
    io_conf.pull_up_en = false;
    io_conf.pin_bit_mask = (1ULL << RS_PIN);
    gpio_config(&io_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(RS_PIN, restart_reset_random_isr, (void *)&RS_PIN); // activate the isr for RS_pin

    // init restart_reset_task
    xTaskCreate(restart_reset_Task, "restart_reset_Task", 4096, NULL, 1, &rsHandler);
}

static void INIT_RAND_PIN(void)
{
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_down_en = true;
    io_conf.pull_up_en = false;
    io_conf.pin_bit_mask = (1ULL << RAND_PIN);
    gpio_config(&io_conf);
    gpio_isr_handler_add(RAND_PIN, restart_reset_random_isr, (void *)&RAND_PIN); // activate the isr for RS_pin

    // init random_activate_task
    xTaskCreate(random_activate_Task, "random_activate_Task", 4096, NULL, 1, &randHandler);
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
    INIT_RS_PIN(); // initialize the reset button

    ap_config_t local_config; // ssid store sample
    wifi_init();
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
            RESTART_WIFI(STA_mode);
            if (!STA_RESTART)
                esp_restart();
            xSemaphoreGive(xSEMA); // activate the 'serial_operation_functionality'
            INIT_RAND_PIN();       // initialize the random_activate
        }
    }
    else
    {
        ESP_LOGW("AP_connect", "NO_LOCAL_SSID found.... Re-enter local network");
        wifi_connect_ap("ESP-32_local");
        if (!AP_RESTART)
            esp_restart();
        // activate serial using nvs storage
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
    int FreeHeap = esp_get_free_heap_size();
    int DRam = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    int IRam = heap_caps_get_free_size(MALLOC_CAP_32BIT) - heap_caps_get_free_size(MALLOC_CAP_8BIT);
    int LargestFreeHeap = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    ESP_LOGI("HEAP_SIZE", "%d", FreeHeap);
    ESP_LOGI("FREE_DRAM", "%d", DRam / 1024);
    ESP_LOGI("FREE_IRAM", "%d", IRam / 1024);
    ESP_LOGI("FREE_HEAP", "%d", LargestFreeHeap);
}
