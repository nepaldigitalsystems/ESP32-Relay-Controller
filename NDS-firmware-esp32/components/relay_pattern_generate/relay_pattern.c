#include <stdio.h>
#include <string.h>

#include "DATA_FIELD.h"
#include "driver/timer.h"
#include "driver/gpio.h"
// #include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"

/* Relay ON - OFF */
#define NUM_OF_RELAY 16
#define NUM_OF_LED_RELAY 12
#define R_ON 0
#define R_OFF 1

/*******************************************************************************
 *                          Static Data Definitions
 *******************************************************************************/
uint8_t Relay_Status_Value[RELAY_UPDATE_MAX] = {0, R_OFF, R_OFF, R_OFF, R_OFF, R_OFF, R_OFF, R_OFF, R_OFF, R_OFF, R_OFF, R_OFF, R_OFF, R_OFF, R_OFF, R_OFF, R_OFF, 0, 0}; // 0-18
uint8_t Relay_Update_Success[RELAY_UPDATE_MAX] = {0};                                                                                                                     // 0-18
gpio_num_t REALY_PINS[NUM_OF_RELAY] = {GPIO_NUM_13, GPIO_NUM_12, GPIO_NUM_14, GPIO_NUM_27, GPIO_NUM_26, GPIO_NUM_25, GPIO_NUM_33, GPIO_NUM_32, GPIO_NUM_15, GPIO_NUM_4, GPIO_NUM_5, GPIO_NUM_18, GPIO_NUM_19, GPIO_NUM_21, GPIO_NUM_22, GPIO_NUM_23};

esp_timer_handle_t esp_timer_handle1; // timer1 for indipendent serial pattern
esp_timer_handle_t esp_timer_handle2; // timer2 for indipendent random pattern
static bool PATTERN = 0;              // a variable for random_state ; default = 0
static uint8_t COMBINATION = 0;       // a variable for random_state ; default = 0
static xSemaphoreHandle xSEMA = NULL; // global [used by only "relay_pattern.c"]

/*******************************************************************************
 *                          Function Definitions
 *******************************************************************************/

/**
 * @brief Secondary-Task that get activated as a callback for serial_timer within "serial pattern generator" task.
 *
 */
void Serial_Timer_Callback(void *params) // Callback triggered every 1000mS -> 1s
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
    }
    (dir) ? position++ : position--; // increases the pointer position by 1 every second
}

/**
 * @brief Secondary-Task that get activated as a callback for random_timer within "random pattern generator" task.
 *
 */
void Random_Timer_Callback(void *params) // Callback triggered every 1000mS -> 1s
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
    {
        random_seconds = 0;
    }
}

/**
 * @brief Function that get activates serial_pattern_timer1.
 *
 * @param state A boolean value indicating : [1-Start serial_timer1 ; 0-Stop serial_timer1]
 */
static void Serial_Patttern_generator(bool state) // receiver // Serial_timer start or stop
{
    switch (state)
    {
    case 1: // 1 => Start the timer
        esp_timer_start_periodic(esp_timer_handle1, 1000000);
        break;
    case 0: // 0 => stop timer
        esp_timer_stop(esp_timer_handle1);
        break;
    }
}

/**
 * @brief Function that activates a random_pattern_timer2.
 *
 * @param comb 1-Byte value indicating current combination choice for random_pattern
 */
static void Random_Pattern_generator(uint8_t comb) // Random_timer start or stop
{
    static esp_err_t isOn2 = ESP_FAIL;
    if (comb != 0)
    {
        COMBINATION = comb - 1; // determine the COMBINATION {0,1,2,3,4}
        if (ESP_OK == isOn2)    // Before starting ; stop timer once
        {
            esp_timer_stop(esp_timer_handle2);
            isOn2 = ESP_FAIL;
        }
        isOn2 = esp_timer_start_periodic(esp_timer_handle2, 1000000); // Start timer
    }
    else // 0 => stop timer
    {
        if (ESP_OK == isOn2) // stop timer
        {
            esp_timer_stop(esp_timer_handle2);
            isOn2 = ESP_FAIL;
        }
    }
}

/**
 * @brief Task to update relay_switch_states according to internally stored values.
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
                if (ESP_OK == nvs_open("Relay_Status", NVS_READWRITE, &update))
                {
                    uint8_t val = 0;
                    if (ESP_ERR_NVS_NOT_FOUND == nvs_get_u8(update, "random", &val))
                    {
                        Relay_Status_Value[RANDOM_UPDATE] = 0;
                    } // random status value must be ->0 (default)
                    else
                    {
                        Relay_Status_Value[RANDOM_UPDATE] = val;
                    } // 0,1,2,3,4

                    if (ESP_ERR_NVS_NOT_FOUND == nvs_get_u8(update, "serial", &val))
                    {
                        Relay_Status_Value[SERIAL_UPDATE] = 0;
                    } // serial status value must be ->0 (default)
                    else
                    {
                        Relay_Status_Value[SERIAL_UPDATE] = val;
                    } // 1,0

                    for (uint8_t i = 1; i <= RELAY_UPDATE_16; i++) // get "1/0" -> relay_status [1-16]
                    {
                        char str[10];
                        memset(str, 0, sizeof(str));
                        sprintf(str, "Relay%u", i);
                        if (ESP_ERR_NVS_NOT_FOUND == nvs_get_u8(update, str, &val))
                        {
                            Relay_Status_Value[i] = R_OFF;
                        } // need to be inverted
                        else
                        {
                            Relay_Status_Value[i] = val;
                        } // 1,0
                    }
                    nvs_close(update); // no commits to avoid changes
                }

                /****************************************************************************************************************************/
                // Generate the "update_success_status" for each button
                if ((0 == Relay_Status_Value[SERIAL_UPDATE]) && (0 == Relay_Status_Value[RANDOM_UPDATE]))
                {
                    ESP_LOGI("Activate", " --> BTNS ONLY");
                    Serial_Patttern_generator(0); // de-activate serial pattern
                    Random_Pattern_generator(0);  // de-activate random pattern
                    for (int i = 0; i < NUM_OF_RELAY; i++)
                    { // NOTE:- set relay_update_success = '1', if the gpio_set is successful // may need to invert the gpio_set_level
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
                    if ((1 == Relay_Status_Value[SERIAL_UPDATE]) && (0 == Relay_Status_Value[RANDOM_UPDATE]))
                    {
                        Random_Pattern_generator(0); // de-activate random pattern
                        ESP_LOGI("Activate", " --> SERIAL PATTERN");
                        Relay_Update_Success[SERIAL_UPDATE] = 1;
                        Serial_Patttern_generator(1); // activate serial pattern
                    }
                    else if ((Relay_Status_Value[RANDOM_UPDATE] > 0) && (0 == Relay_Status_Value[SERIAL_UPDATE]))
                    {
                        Serial_Patttern_generator(0); // de-activate serial pattern
                        ESP_LOGI("Activate", " --> RANDOM PATTERN : %d", Relay_Status_Value[RANDOM_UPDATE]);
                        Relay_Update_Success[RANDOM_UPDATE] = 1;
                        Random_Pattern_generator(Relay_Status_Value[RANDOM_UPDATE]); // call a function to generate random patterns
                    }
                    else
                    {
                        Serial_Patttern_generator(0); // de-activate serial pattern
                        Random_Pattern_generator(0);  // de-activate random pattern
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
 * @brief Function to give Semaphore ; which inturn activates a task [for relay_state display]
 */
void Activate_Relays()
{
    xSemaphoreGive(xSEMA);
}

/**
 * @brief Function to create/initialize tasks for relay_update, Random_pattern and Serial_pattern
 */
void setup_relay_update_task()
{
    xSEMA = xSemaphoreCreateBinary();                                             // semaphore for Relay_switch_update task
    xTaskCreate(Relay_switch_update, "Relay_switch_update", 4096, NULL, 6, NULL); // sender // higher priority
}
