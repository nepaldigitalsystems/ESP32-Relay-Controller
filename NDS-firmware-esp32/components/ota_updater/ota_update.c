#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
// #include <sys/time.h>
#include "esp_sntp.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_ota_ops.h"

#define TAG "OTA_TAG"
#define CONFIG_SNTP_TIME_SYNC_METHOD_IMMED 1

static xSemaphoreHandle ota_semaphore;
static esp_timer_handle_t esp_OTAtimer_handle;

const int software_version = 1;
const char google_cert[] = "MIIFWjCCA0KgAwIBAgIQbkepxUtHDA3sM9CJuRz04TANBgkqhkiG9w0BAQwFADBHMQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzEUMBIGA1UEAxMLR1RTIFJvb3QgUjEwHhcNMTYwNjIyMDAwMDAwWhcNMzYwNjIyMDAwMDAwWjBHMQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzEUMBIGA1UEAxMLR1RTIFJvb3QgUjEwggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAwggIKAoICAQC2EQKLHuOhd5s73L+UPreVp0A8of2C+X0yBoJx9vaMf/vo27xqLpeXo4xL+Sv2sfnOhB2x+cWX3u+58qPpvBKJXqeqUqv4IyfLpLGcY9vXmX7wCl7raKb0xlpHDU0QM+NOsROjyBhsS+z8CZDfnWQpJSMHobTSPS5g4M/SCYe7zUjwTcLCeoiKu7rPWRnWr4+wB7CeMfGCwcDfLqZtbBkOtdh+JhpFAz2weaSUKK0PfyblqAj+lug8aJRT7oM6iCsVlgmy4HqMLnXWnOunVmSPlk9orj2XwoSPwLxAwAtcvfaHszVsrBhQf4TgTM2S0yDpM7xSma8ytSmzJSq0SPly4cpk9+aCEI3oncKKiPo4Zor8Y/kB+Xj9e1x3+naH+uzfsQ55lVe0vSbv1gHR6xYKu44LtcXFilWr06zqkUspzBmkMiVOKvFlRNACzqrOSbTqn3yDsEB750Orp2yjj32JgfpMpf/VjsPOS+C12LOORc92wO1AK/1TD7Cn1TsNsYqiA94xrcx36m97PtbfkSIS5r762DL8EGMUUXLeXdYWk70paDPvOmbsB4om3xPXV2V4J95eSRQAogB/mqghtqmxlbCluQ0WEdrHbEg8QOB+DVrNVjzRlwW5y0vtOUucxD/SVRNuJLDWcfr0wbrM7Rv1/oFB2ACYPTrIrnqYNxgFlQIDAQABo0IwQDAOBgNVHQ8BAf8EBAMCAQYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQU5K8rJnEaK0gnhS9SZizv8IkTcT4wDQYJKoZIhvcNAQEMBQADggIBADiWCu49tJYeX++dnAsznyvgyv3SjgofQXSlfKqE1OXyHuY3UjKcC9FhHb8owbZEKTV1d5iyfNm9dKyKaOOpMQkpAWBz40d8U6iQSifvS9efk+eCNs6aaAyC58/UEBZvXw6ZXPYfcX3v73svfuo21pdwCxXu11xWajOl40k4DLh9+42FpLFZXvRq4d2h9mREruZRgyFmxhE+885H7pwoHyXa/6xmld01D1zvICxi/ZG6qcz8WpyTgYMpl0p8WnK0OdC3d8t5/Wk6kjftbjhlRn7pYL15iJdfOBL07q9bgsiG1eGZbYwE8na6SfZu6W0eX6DvJ4J2QPim01hcDyxC2kLGe4g0x8HYRZvBPsVhHdljUEn2NIVq4BjFbkerQUIpm/ZgDdIx02OYI5NaAIFItO/Nis3Jz5nu2Z6qNuFoS3FJFDYoOj0dzpqPJeaAcWErtXvM+SUWgeExX6GjfhaknBZqlxi9dnKlC54dNuYvoS++cJEPqOba+MSSQGwlfnuzCdyyF62ARPBopY+Udf90WuioAnwMCeKpSwughQtiue+hMZL77/ZRBIls6Kl0obsXs7X9SQ98POyDGCBDTtWTurQ0sR8WNh8M5mQ5Fkzc4P4dyKliPUDqysU0ArSuiYgzNdwsE3PYJ/HQcu51OyLemGhmW/HGY0dVHLqlCFF1pkgl";
// extern const uint8_t server_cert_pem_start[] asm("_binary_google_crt_start"); // include from flash

esp_err_t client_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    default:
        break;
    }
    return ESP_OK;
}
static esp_err_t validate_image_header(esp_app_desc_t *incoming_ota_desc)
{
    // get version of current partition
    const esp_partition_t *running_partition = esp_ota_get_running_partition();
    esp_app_desc_t running_partition_description;
    esp_ota_get_partition_description(running_partition, &running_partition_description);

    ESP_LOGI(TAG, "current version is %s\n", running_partition_description.version);
    ESP_LOGI(TAG, "new version is %s\n", incoming_ota_desc->version);

    if (strcmp(running_partition_description.version, incoming_ota_desc->version) == 0)
    {
        ESP_LOGW(TAG, "NEW VERSION IS THE SAME AS CURRENT VERSION. ABORTING");
        return ESP_FAIL;
    }
    return ESP_OK;
}

// static void obtain_time(void);

static void check_ota_update_task(void *data)
{
    // Start timer along with task
    if (ESP_OK != esp_timer_start_periodic(esp_OTAtimer_handle, 10000000))
    {
        esp_timer_stop(esp_OTAtimer_handle);
        ESP_LOGW("RESTARTING_OTA_TIMER", "...");
        esp_timer_start_periodic(esp_OTAtimer_handle, 10000000); // 10 Sec
    }

    esp_http_client_config_t clientConfig = {
        .url = "https://drive.google.com/drive/folders/1J7KOADvUfDt9aP9RwIdYQqFd3yoVoKP6", // our ota location
        .event_handler = client_event_handler,
        .cert_pem = google_cert // if CA cert of URLs differ then it should be appended to `cert_pem` member of `http_config`
    };

    // condition to check every two days
    for (;;)
    {
        if (NULL != ota_semaphore)
        {
            if (xSemaphoreTake(ota_semaphore, portMAX_DELAY))
            {
                ESP_LOGI(TAG, "Invoking OTA...");

                // if (ESP_OK == esp_https_ota(&clientConfig))
                // {
                //     // download the new bin file and copy to ota sector
                //     ESP_LOGI(TAG, "OTA flash succsessfull for version %d. Restarting...", software_version);
                //     vTaskDelay(500 / portTICK_PERIOD_MS);
                //     esp_restart();
                // }
                // else
                // {
                //     ESP_LOGE(TAG, "Failed to update firmware");
                // }

                esp_https_ota_config_t ota_config = {
                    .http_config = &clientConfig};

                esp_https_ota_handle_t ota_handle = NULL;
                if (ESP_OK != esp_https_ota_begin(&ota_config, &ota_handle))
                {
                    ESP_LOGE(TAG, "esp_https_ota_begin failed");
                    continue;
                }
                esp_app_desc_t incoming_ota_desc = {0}; // clearing the structure
                if (ESP_OK != esp_https_ota_get_img_desc(ota_handle, &incoming_ota_desc))
                {
                    ESP_LOGE(TAG, "esp_https_ota_get_img_desc failed");
                    ESP_ERROR_CHECK(esp_https_ota_finish(ota_handle));
                    continue;
                }

                if (ESP_OK != validate_image_header(&incoming_ota_desc))
                {
                    ESP_LOGE(TAG, "validate_image_header failed");
                    ESP_ERROR_CHECK(esp_https_ota_finish(ota_handle));
                    continue;
                }

                while (true)
                {
                    if (ESP_ERR_HTTPS_OTA_IN_PROGRESS != esp_https_ota_perform(ota_handle))
                    {
                        break;
                    }
                }

                if (ESP_OK == esp_https_ota_finish(ota_handle))
                {
                    printf("restarting..\n");
                    vTaskDelay(pdMS_TO_TICKS(2000));
                    esp_restart();
                }
                else
                {
                    ESP_LOGE(TAG, "esp_https_ota_finish failed");
                }
            }
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void Activate_OTA()
{
    xSemaphoreGive(ota_semaphore);
}

static void OTA_Timer_Callback(void *params)
{
    ESP_LOGI(TAG, "Notification of Timer_OTA_checkup");
    // check periodic timing and call
    // get_time()
    // Activate_OTA();
}

void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Notification of a time synchronization event");
}

static void obtain_time(void)
{
    // ESP_ERROR_CHECK(nvs_flash_init());
    // ESP_ERROR_CHECK(esp_netif_init());
    // ESP_ERROR_CHECK(esp_event_loop_create_default());

    /**
     * NTP server address could be aquired via DHCP,
     * see LWIP_DHCP_GET_NTP_SRV menuconfig option
     */
#ifdef LWIP_DHCP_GET_NTP_SRV
    sntp_servermode_dhcp(1);
#endif

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    // ESP_ERROR_CHECK(example_connect());

    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    sntp_init();

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

    // ESP_ERROR_CHECK(example_disconnect());
}
void sync_time()
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    // Is time set? If not, tm_year will be (1970 - 1900).
    if (timeinfo.tm_year < (2016 - 1900))
    {
        ESP_LOGI(TAG, "Time is not set yet. Connecting to WiFi and getting time over NTP.");
        obtain_time();
        // update 'now' variable with current time
        time(&now);
    }
    char strftime_buf[64];
    // Set timezone to Nepal Standard Time
    setenv("TZ", "UTC+05:45", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time in NEPAL is: %s", strftime_buf);
}

void initialize_ota_setup()
{
    // sync time
    // sync_time();
    /***************************************************************/
    // initialize a timer for OTA
    const esp_timer_create_args_t esp_OTAtimer_create_args = {
        .callback = OTA_Timer_Callback,
        .name = "OTA timer"};
    esp_timer_create(&esp_OTAtimer_create_args, &esp_OTAtimer_handle);

    /***************************************************************/
    ota_semaphore = xSemaphoreCreateBinary();
    xTaskCreate(check_ota_update_task, "check_ota_update_task", 4096, NULL, 3, NULL);
    /***************************************************************/
}
