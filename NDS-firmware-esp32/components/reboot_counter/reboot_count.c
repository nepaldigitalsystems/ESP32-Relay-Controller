#include <stdio.h>
#include "DATA_FIELD.h"
// #include "nvs.h"
#include "nvs_flash.h"
#include "driver/timer.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

/*******************************************************************************
 *                          Static Data Definitions
 *******************************************************************************/
static bool AP_RESTART = false;  // a variable that remembers to -> Restart once in AP-mode at initialization stage
static bool STA_RESTART = false; // a variable that remembers to -> Restart once in STA-mode at initialization stage

/*******************************************************************************
 *                          Function Definitions
 *******************************************************************************/
/**
 * @brief Function to provide static boolean variable "AP_RESTART" [from reboot_count.c]
 */
void get_AP_RESTART()
{
    return AP_RESTART;
}

/**
 * @brief Function to provide static boolean variable "STA_RESTART" [from reboot_count.c]
 */
void get_STA_RESTART()
{
    return STA_RESTART;
}

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
