#include <stdio.h>
#include "DATA_FIELD.h"
// #include "nvs.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_intr_alloc.h"

/*******************************************************************************
 *                          Static Data Definitions
 *******************************************************************************/
static bool SERIAL_PENDING = false;     // variable to tackle a blocking condition, that occurs when random button [on the remote control], gets pressed during serial pattern operation.
static gpio_config_t io_conf;           // for multiple gpio configuration
static TaskHandle_t rsHandler = NULL;   // handler to notify, reset_restart interrupt button being pressed
static TaskHandle_t randHandler = NULL; // handler to notify, random interrupt button being pressed

/* random_pin, restart_reset and system_led pin declarations */
const uint32_t RS_PIN = 34;
const uint32_t RAND_PIN = 35;

/*******************************************************************************
 *                          Static Function Definitions
 *******************************************************************************/
/**
 * @brief Task to update relay_switch_states according to internally stored values.
 *
 * @param args GPIO_Pin_number passed to invoke corresponding action
 */
void IRAM_ATTR restart_reset_random_isr(void *args)
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
 * @brief Function to initailized restart_reset interrupt pin.
 *
 */
void INIT_RS_PIN()
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
void INIT_RAND_PIN()
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

/**
 * @brief Task for restart_reset_button operation.
 *
 */
static void restart_reset_Task(void *params)
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
static void random_activate_Task(void *params)
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
                Activate_Relays(); // gives semaphore from relay_pattern.h
            }
            ESP_LOGE("SYSTEM_LED", "ON");
            gpio_set_level(SYS_LED, SYS_LED_ON);                                         // turn-ON system led
            gpio_isr_handler_add(RAND_PIN, restart_reset_random_isr, (void *)&RAND_PIN); // re-enable the interrupt
        }
    }
    vTaskDelete(NULL);
}
