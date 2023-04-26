#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "nvs_flash.h"
#include "esp_sntp.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_ota_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "cJSON.h"

#define TAG "OTA_TAG"
#define UPDATE_JSON_URL ".../firmware.json"
// #define OTA_URL "https://nepaldigisys.com/static/firmware/ESP32_Relay.bin"

// receive buffer
char rcv_buffer[200];

// #define CONFIG_SNTP_TIME_SYNC_METHOD_IMMED 1

static float FIRMWARE_VERSION = 0;

static xSemaphoreHandle ota_semaphore;
static esp_timer_handle_t esp_OTAtimer_handle;

const char server_cert[] = "-----BEGIN CERTIFICATE-----\r\n"
                           "MIIOPTCCDSWgAwIBAgIRANVRkRZmSKIBCiVhv69JoOYwDQYJKoZIhvcNAQELBQAw\r\n"
                           "RjELMAkGA1UEBhMCVVMxIjAgBgNVBAoTGUdvb2dsZSBUcnVzdCBTZXJ2aWNlcyBM\r\n"
                           "TEMxEzARBgNVBAMTCkdUUyBDQSAxQzMwHhcNMjMwNDAzMDgxNzU4WhcNMjMwNjI2\r\n"
                           "MDgxNzU3WjAXMRUwEwYDVQQDDAwqLmdvb2dsZS5jb20wWTATBgcqhkjOPQIBBggq\r\n"
                           "hkjOPQMBBwNCAAQuH7/mN1E3fr4AT/buHz+qSaoSaZxWbrhmyY4Q+0AnYHTt+qVH\r\n"
                           "952+RpqnWjBdr9OIpCVFnYqKge2zvL8AFPT1o4IMHjCCDBowDgYDVR0PAQH/BAQD\r\n"
                           "AgeAMBMGA1UdJQQMMAoGCCsGAQUFBwMBMAwGA1UdEwEB/wQCMAAwHQYDVR0OBBYE\r\n"
                           "FET6sq8x/8nmwD/gz34idirjZJfKMB8GA1UdIwQYMBaAFIp0f6+Fze6VzT2c0OJG\r\n"
                           "FPNxNR0nMGoGCCsGAQUFBwEBBF4wXDAnBggrBgEFBQcwAYYbaHR0cDovL29jc3Au\r\n"
                           "cGtpLmdvb2cvZ3RzMWMzMDEGCCsGAQUFBzAChiVodHRwOi8vcGtpLmdvb2cvcmVw\r\n"
                           "by9jZXJ0cy9ndHMxYzMuZGVyMIIJzQYDVR0RBIIJxDCCCcCCDCouZ29vZ2xlLmNv\r\n"
                           "bYIWKi5hcHBlbmdpbmUuZ29vZ2xlLmNvbYIJKi5iZG4uZGV2ghUqLm9yaWdpbi10\r\n"
                           "ZXN0LmJkbi5kZXaCEiouY2xvdWQuZ29vZ2xlLmNvbYIYKi5jcm93ZHNvdXJjZS5n\r\n"
                           "b29nbGUuY29tghgqLmRhdGFjb21wdXRlLmdvb2dsZS5jb22CCyouZ29vZ2xlLmNh\r\n"
                           "ggsqLmdvb2dsZS5jbIIOKi5nb29nbGUuY28uaW6CDiouZ29vZ2xlLmNvLmpwgg4q\r\n"
                           "Lmdvb2dsZS5jby51a4IPKi5nb29nbGUuY29tLmFygg8qLmdvb2dsZS5jb20uYXWC\r\n"
                           "DyouZ29vZ2xlLmNvbS5icoIPKi5nb29nbGUuY29tLmNvgg8qLmdvb2dsZS5jb20u\r\n"
                           "bXiCDyouZ29vZ2xlLmNvbS50coIPKi5nb29nbGUuY29tLnZuggsqLmdvb2dsZS5k\r\n"
                           "ZYILKi5nb29nbGUuZXOCCyouZ29vZ2xlLmZyggsqLmdvb2dsZS5odYILKi5nb29n\r\n"
                           "bGUuaXSCCyouZ29vZ2xlLm5sggsqLmdvb2dsZS5wbIILKi5nb29nbGUucHSCEiou\r\n"
                           "Z29vZ2xlYWRhcGlzLmNvbYIPKi5nb29nbGVhcGlzLmNughEqLmdvb2dsZXZpZGVv\r\n"
                           "LmNvbYIMKi5nc3RhdGljLmNughAqLmdzdGF0aWMtY24uY29tgg9nb29nbGVjbmFw\r\n"
                           "cHMuY26CESouZ29vZ2xlY25hcHBzLmNughFnb29nbGVhcHBzLWNuLmNvbYITKi5n\r\n"
                           "b29nbGVhcHBzLWNuLmNvbYIMZ2tlY25hcHBzLmNugg4qLmdrZWNuYXBwcy5jboIS\r\n"
                           "Z29vZ2xlZG93bmxvYWRzLmNughQqLmdvb2dsZWRvd25sb2Fkcy5jboIQcmVjYXB0\r\n"
                           "Y2hhLm5ldC5jboISKi5yZWNhcHRjaGEubmV0LmNughByZWNhcHRjaGEtY24ubmV0\r\n"
                           "ghIqLnJlY2FwdGNoYS1jbi5uZXSCC3dpZGV2aW5lLmNugg0qLndpZGV2aW5lLmNu\r\n"
                           "ghFhbXBwcm9qZWN0Lm9yZy5jboITKi5hbXBwcm9qZWN0Lm9yZy5jboIRYW1wcHJv\r\n"
                           "amVjdC5uZXQuY26CEyouYW1wcHJvamVjdC5uZXQuY26CF2dvb2dsZS1hbmFseXRp\r\n"
                           "Y3MtY24uY29tghkqLmdvb2dsZS1hbmFseXRpY3MtY24uY29tghdnb29nbGVhZHNl\r\n"
                           "cnZpY2VzLWNuLmNvbYIZKi5nb29nbGVhZHNlcnZpY2VzLWNuLmNvbYIRZ29vZ2xl\r\n"
                           "dmFkcy1jbi5jb22CEyouZ29vZ2xldmFkcy1jbi5jb22CEWdvb2dsZWFwaXMtY24u\r\n"
                           "Y29tghMqLmdvb2dsZWFwaXMtY24uY29tghVnb29nbGVvcHRpbWl6ZS1jbi5jb22C\r\n"
                           "FyouZ29vZ2xlb3B0aW1pemUtY24uY29tghJkb3VibGVjbGljay1jbi5uZXSCFCou\r\n"
                           "ZG91YmxlY2xpY2stY24ubmV0ghgqLmZscy5kb3VibGVjbGljay1jbi5uZXSCFiou\r\n"
                           "Zy5kb3VibGVjbGljay1jbi5uZXSCDmRvdWJsZWNsaWNrLmNughAqLmRvdWJsZWNs\r\n"
                           "aWNrLmNughQqLmZscy5kb3VibGVjbGljay5jboISKi5nLmRvdWJsZWNsaWNrLmNu\r\n"
                           "ghFkYXJ0c2VhcmNoLWNuLm5ldIITKi5kYXJ0c2VhcmNoLWNuLm5ldIIdZ29vZ2xl\r\n"
                           "dHJhdmVsYWRzZXJ2aWNlcy1jbi5jb22CHyouZ29vZ2xldHJhdmVsYWRzZXJ2aWNl\r\n"
                           "cy1jbi5jb22CGGdvb2dsZXRhZ3NlcnZpY2VzLWNuLmNvbYIaKi5nb29nbGV0YWdz\r\n"
                           "ZXJ2aWNlcy1jbi5jb22CF2dvb2dsZXRhZ21hbmFnZXItY24uY29tghkqLmdvb2ds\r\n"
                           "ZXRhZ21hbmFnZXItY24uY29tghhnb29nbGVzeW5kaWNhdGlvbi1jbi5jb22CGiou\r\n"
                           "Z29vZ2xlc3luZGljYXRpb24tY24uY29tgiQqLnNhZmVmcmFtZS5nb29nbGVzeW5k\r\n"
                           "aWNhdGlvbi1jbi5jb22CFmFwcC1tZWFzdXJlbWVudC1jbi5jb22CGCouYXBwLW1l\r\n"
                           "YXN1cmVtZW50LWNuLmNvbYILZ3Z0MS1jbi5jb22CDSouZ3Z0MS1jbi5jb22CC2d2\r\n"
                           "dDItY24uY29tgg0qLmd2dDItY24uY29tggsybWRuLWNuLm5ldIINKi4ybWRuLWNu\r\n"
                           "Lm5ldIIUZ29vZ2xlZmxpZ2h0cy1jbi5uZXSCFiouZ29vZ2xlZmxpZ2h0cy1jbi5u\r\n"
                           "ZXSCDGFkbW9iLWNuLmNvbYIOKi5hZG1vYi1jbi5jb22CFGdvb2dsZXNhbmRib3gt\r\n"
                           "Y24uY29tghYqLmdvb2dsZXNhbmRib3gtY24uY29tgh4qLnNhZmVudXAuZ29vZ2xl\r\n"
                           "c2FuZGJveC1jbi5jb22CDSouZ3N0YXRpYy5jb22CFCoubWV0cmljLmdzdGF0aWMu\r\n"
                           "Y29tggoqLmd2dDEuY29tghEqLmdjcGNkbi5ndnQxLmNvbYIKKi5ndnQyLmNvbYIO\r\n"
                           "Ki5nY3AuZ3Z0Mi5jb22CECoudXJsLmdvb2dsZS5jb22CFioueW91dHViZS1ub2Nv\r\n"
                           "b2tpZS5jb22CCyoueXRpbWcuY29tggthbmRyb2lkLmNvbYINKi5hbmRyb2lkLmNv\r\n"
                           "bYITKi5mbGFzaC5hbmRyb2lkLmNvbYIEZy5jboIGKi5nLmNuggRnLmNvggYqLmcu\r\n"
                           "Y2+CBmdvby5nbIIKd3d3Lmdvby5nbIIUZ29vZ2xlLWFuYWx5dGljcy5jb22CFiou\r\n"
                           "Z29vZ2xlLWFuYWx5dGljcy5jb22CCmdvb2dsZS5jb22CEmdvb2dsZWNvbW1lcmNl\r\n"
                           "LmNvbYIUKi5nb29nbGVjb21tZXJjZS5jb22CCGdncGh0LmNuggoqLmdncGh0LmNu\r\n"
                           "ggp1cmNoaW4uY29tggwqLnVyY2hpbi5jb22CCHlvdXR1LmJlggt5b3V0dWJlLmNv\r\n"
                           "bYINKi55b3V0dWJlLmNvbYIUeW91dHViZWVkdWNhdGlvbi5jb22CFioueW91dHVi\r\n"
                           "ZWVkdWNhdGlvbi5jb22CD3lvdXR1YmVraWRzLmNvbYIRKi55b3V0dWJla2lkcy5j\r\n"
                           "b22CBXl0LmJlggcqLnl0LmJlghphbmRyb2lkLmNsaWVudHMuZ29vZ2xlLmNvbYIb\r\n"
                           "ZGV2ZWxvcGVyLmFuZHJvaWQuZ29vZ2xlLmNughxkZXZlbG9wZXJzLmFuZHJvaWQu\r\n"
                           "Z29vZ2xlLmNughhzb3VyY2UuYW5kcm9pZC5nb29nbGUuY24wIQYDVR0gBBowGDAI\r\n"
                           "BgZngQwBAgEwDAYKKwYBBAHWeQIFAzA8BgNVHR8ENTAzMDGgL6AthitodHRwOi8v\r\n"
                           "Y3Jscy5wa2kuZ29vZy9ndHMxYzMvUXFGeGJpOU00OGMuY3JsMIIBBQYKKwYBBAHW\r\n"
                           "eQIEAgSB9gSB8wDxAHcArfe++nz/EMiLnT2cHj4YarRnKV3PsQwkyoWGNOvcgooA\r\n"
                           "AAGHRmi5jAAABAMASDBGAiEA5GT6zLffOV9nFyH5KVgUjM1NSOh2z56H5CKh61eM\r\n"
                           "/V8CIQDDVDOJQ/gn2SXHlM2f+r9dDoOOWMEJ0si7+Y02USY4sgB2ALNzdwfhhFD4\r\n"
                           "Y4bWBancEQlKeS2xZwwLh9zwAw55NqWaAAABh0ZouWsAAAQDAEcwRQIgWa/o6iit\r\n"
                           "+yyx1/Ec2gT0zI6+CpKNbPaDRkMCpowgfYQCIQCfxCmN2eUJgrTKUeDS1rzDi3qe\r\n"
                           "9t/IwUFnI/ObYEqXsTANBgkqhkiG9w0BAQsFAAOCAQEA2ohY9gl5/nJbzOMfZGbI\r\n"
                           "BpOM0wtbnQk95p4IhmfV1G3WSvs9Myyf/EwgAT9ljD3UcZKcQqgC4txXbaV6xyLh\r\n"
                           "g0xQHPIL/IcfRHtiYtksYg36WrctBUdStumlwxQYc7TIfHaUffO42N3zNCTVT9Il\r\n"
                           "5ojPi1dYqP7yoSXsuqOKpVH7D827zv4vvMA6t0nYM49Pa7rXYrDuY3yrOgGml+7V\r\n"
                           "93UBNTDXxbbiWAiAc4C+K+xb36PVtn3Q24Okd8j1ABy012/IXiZM9pZbklIbW9EP\r\n"
                           "ndS9zxK191MlzkkL10nURP99f0L7LxABFjKwIFJXntrhtkcmi09rU65um5PhT0Gq\r\n"
                           "8w==\r\n"
                           "-----END CERTIFICATE-----\r\n";
// extern const uint8_t server_cert_pem_start[] asm("_binary_google2_crt_start"); // include from flash
// extern const uint8_t server_cert_pem_end[] asm("_binary_google2_crt_end");

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
        if (!esp_http_client_is_chunked_response(evt->client))
        {
            strncpy(rcv_buffer, (char *)evt->data, evt->data_len);
        }
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
// static esp_err_t validate_image_header(esp_app_desc_t *incoming_ota_desc)
// {
//     // get version of current partition
//     const esp_partition_t *running_partition = esp_ota_get_running_partition();
//     esp_app_desc_t running_partition_description;
//     esp_ota_get_partition_description(running_partition, &running_partition_description);

//     ESP_LOGI(TAG, "current version is %s\n", running_partition_description.version);
//     ESP_LOGI(TAG, "new version is %s\n", incoming_ota_desc->version);

//     if (strcmp(running_partition_description.version, incoming_ota_desc->version) == 0)
//     {
//         ESP_LOGW(TAG, "NEW VERSION IS THE SAME AS CURRENT VERSION. ABORTING");
//         return ESP_FAIL;
//     }
//     return ESP_OK;
// }

// static void obtain_time(void);
static void check_ota_update_task(void *data)
{
    // Start timer along with task
    if (ESP_OK != esp_timer_start_periodic(esp_OTAtimer_handle, 30000000))
    {
        esp_timer_stop(esp_OTAtimer_handle);
        ESP_LOGW("RESTARTING_OTA_TIMER", "...");
        esp_timer_start_periodic(esp_OTAtimer_handle, 30000000); // 30 Sec
    }

    // condition to check every two days
    for (;;)
    {
        if (NULL != ota_semaphore)
        {
            if (xSemaphoreTake(ota_semaphore, portMAX_DELAY))
            {
                ESP_LOGI(TAG, "Looking for new firmware...OTA...");

                esp_http_client_config_t config = {
                    .url = UPDATE_JSON_URL, // our ota location
                    .event_handler = client_event_handler,
                    // .cert_pem = NULL, //(char *)server_cert_pem_start //// if CA cert of URLs differ then it should be appended to `cert_pem` member of `http_config`
                };
                esp_http_client_handle_t client = esp_http_client_init(&config);

                // downloading the json file
                if (ESP_OK == esp_http_client_perform(client))
                {
                    // parse the json file
                    cJSON *json = cJSON_Parse(rcv_buffer);
                    if (json == NULL)
                    {
                        ESP_LOGW(TAG, "downloaded file is not a valid json, aborting...\n");
                    }
                    else
                    {
                        cJSON *version = cJSON_GetObjectItemCaseSensitive(json, "version");
                        cJSON *file = cJSON_GetObjectItemCaseSensitive(json, "file");
                        // check the version
                        if (!cJSON_IsNumber(version))
                        {
                            ESP_LOGW(TAG, "unable to read new version, aborting...\n");
                        }
                        else
                        {
                            float new_version = version->valuedouble;
                            if (new_version > FIRMWARE_VERSION)
                            {
                                ESP_LOGW(TAG, "current firmware version (%.1f) is lower than the available one (%.1f), upgrading...\n", FIRMWARE_VERSION, new_version);
                                if (cJSON_IsString(file) && (file->valuestring != NULL))
                                {
                                    ESP_LOGW(TAG, "downloading and installing new firmware (%s)...\n", file->valuestring);

                                    esp_http_client_config_t ota_client_config = {
                                        .url = file->valuestring,
                                        .cert_pem = server_cert,
                                    };
                                    esp_err_t ret = esp_https_ota(&ota_client_config);
                                    if (ret == ESP_OK)
                                    {
                                        ESP_LOGW(TAG, "OTA OK, restarting...\n");
                                        FIRMWARE_VERSION = new_version;
                                        esp_restart();
                                    }
                                    else
                                    {
                                        ESP_LOGW(TAG, "OTA failed...\n");
                                    }
                                }
                                else
                                {
                                    ESP_LOGW(TAG, "unable to read the new file name, aborting...\n");
                                }
                            }
                            else
                            {
                                ESP_LOGW(TAG, "current firmware version (%.1f) is greater or equal to the available one (%.1f), nothing to do...\n", FIRMWARE_VERSION, new_version);
                            }
                        }
                    }
                }
                else
                {
                    ESP_LOGW(TAG, "unable to download the json file, aborting...\n");
                }
                // cleanup
                esp_http_client_cleanup(client);

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

                // esp_https_ota_config_t ota_config = {
                //     .http_config = &clientConfig};

                // esp_https_ota_handle_t ota_handle = NULL;
                // if (ESP_OK != esp_https_ota_begin(&ota_config, &ota_handle))
                // {
                //     ESP_LOGE(TAG, "esp_https_ota_begin failed");
                //     continue;
                // }
                // esp_app_desc_t incoming_ota_desc = {0}; // clearing the structure
                // if (ESP_OK != esp_https_ota_get_img_desc(ota_handle, &incoming_ota_desc))
                // {
                //     ESP_LOGE(TAG, "esp_https_ota_get_img_desc failed");
                //     ESP_ERROR_CHECK(esp_https_ota_finish(ota_handle));
                //     continue;
                // }

                // if (ESP_OK != validate_image_header(&incoming_ota_desc))
                // {
                //     ESP_LOGE(TAG, "validate_image_header failed");
                //     ESP_ERROR_CHECK(esp_https_ota_finish(ota_handle));
                //     continue;
                // }

                // while (true)
                // {
                //     if (ESP_ERR_HTTPS_OTA_IN_PROGRESS != esp_https_ota_perform(ota_handle)) // downloading the bin file
                //     {
                //         break;
                //     }
                // }

                // if (ESP_OK == esp_https_ota_finish(ota_handle))
                // {
                //     ESP_LOGW(TAG,"restarting..\n");
                //     vTaskDelay(pdMS_TO_TICKS(1000));
                //     esp_restart();
                // }
                // else
                // {
                //     ESP_LOGE(TAG, "esp_https_ota_finish failed");
                // }
            }
        }
    }
    vTaskDelete(NULL);
}

esp_err_t Activate_OTA(void)
{
    esp_err_t res = xSemaphoreGive(ota_semaphore);
    return res;
}

static void OTA_Timer_Callback(void *params)
{
    // generating current date/time
    ESP_LOGI(TAG, "Notification of Timer_OTA_checkup");
    time_t now;
    struct tm current_timeinfo, compile_timeinfo;
    time(&now);
    localtime_r(&now, &current_timeinfo);

    // generating the prev compiled date/time
    const char *compile_date_var = __DATE__;
    const char *compile_time_var = __TIME__;
    strptime(compile_date_var, "%b %d %Y", &compile_timeinfo); // string to time
    strptime(compile_time_var, "%I:%M:%S", &compile_timeinfo); // string to time

    // display respective date/time results
    ESP_LOGE("OTA_TIME_check", "compile__mon/day: %d/%d  vs  current__mon/day: %d/%d", compile_timeinfo.tm_mon, compile_timeinfo.tm_mday, current_timeinfo.tm_mon, current_timeinfo.tm_mday);
    ESP_LOGE("OTA_TIME_check", "compile__time: %d:%d:%d  vs  current__time: %d:%d:%d", compile_timeinfo.tm_hour, compile_timeinfo.tm_min, compile_timeinfo.tm_sec, current_timeinfo.tm_hour, current_timeinfo.tm_min, current_timeinfo.tm_sec);

    // check periodic timing and call
    if (current_timeinfo.tm_mday >= compile_timeinfo.tm_mday)
    {
        if ((current_timeinfo.tm_min - compile_timeinfo.tm_min) > 2) // OTA update after 2 min passes
        {
            // update the compile time
            // ESP_LOGW("OTA_Update", "Activating OTA operation");
            Activate_OTA();
        }
        else
        {
            ESP_LOGW("OTA_Update", "Two min not crossed. Still time some time remaining before OTA is triggred");
        }
    }
    // Activate_OTA();
}

void initialize_ota_setup(void)
{
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
