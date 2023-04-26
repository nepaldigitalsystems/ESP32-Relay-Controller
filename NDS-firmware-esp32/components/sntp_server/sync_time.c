#include <stdio.h>
#include <string.h>
#include <time.h>
#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"

#define TAG "Sntp_Server"

static void obtain_time(void);
static void initialize_sntp(void);

static void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Notification of a time synchronization event");
}

void Get_current_date_time(char *date_time)
{
    char strfdatetime_buf[64]; // both date and time
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // Set timezone to Nepal Standard Time
    setenv("TZ", "UTC-05:45", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    if (NULL != date_time)
    {
        strftime(strfdatetime_buf, sizeof(strfdatetime_buf), "%c", &timeinfo); // both date and time
        strcpy(date_time, strfdatetime_buf);
    }
    // if (NULL != D)
    // {
    //     strftime(strfdate_buf, sizeof(strfdate_buf), "%Y-%m-%d", &timeinfo); // date
    //     strcpy(D, strfdate_buf);
    // }
    // if (NULL != T)
    // {
    //     strftime(strftime_buf, sizeof(strftime_buf), "%I:%M:%S.%p", &timeinfo); // time
    //     strcpy(T, strftime_buf);
    // }
}

static void obtain_time(void)
{
    initialize_sntp();
    // wait for time to be set
    time_t now = 0;
    struct tm timeinfo = {0};
    int retry = 0;
    const int retry_count = 10;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count)
    {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    time(&now);
    localtime_r(&now, &timeinfo);
    if (retry < retry_count)
    {
        ESP_LOGI(TAG, "%s", "NTP time Syncroniztion Successful");
    }
    else
    {
        ESP_LOGW(TAG, "%s", "Couldn't sync the NTP time.... Please restart");
    }
}

void Set_SystemTime_SNTP()
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    // Is time set? If not, tm_year will be (1970 - 1900).
    if (timeinfo.tm_year < (2023 - 1900))
    {
        ESP_LOGI(TAG, "Time is not set yet. Connecting to WiFi and getting time over NTP.");
        obtain_time();
        // update 'now' variable with current time
        time(&now);
    }
}

static void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    sntp_init();
}