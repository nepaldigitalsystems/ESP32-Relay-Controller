#include "stdio.h"
#include "string.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRtos.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_netif.h"
#include "esp_wifi.h"

uint8_t reconnect_count = 0;
// just creating a global reference to station network interface
esp_netif_t *esp_netif = NULL;
// [encapsulate within this file only] ; create event groups for wifi_events
static EventGroupHandle_t wifi_events;

// just to indicate IP state 0/1
static const int CONNECTED_GOT_IP = BIT0;
static const int DISCONNECTED_GOT_IP = BIT1;

extern void start_dns_server(void);
extern void http_server_ap_mode(void);
extern void http_server_sta_mode(void);
extern void Connect_Portal();

static void Set_static_ip(esp_netif_t *);

// return corresponding error
const char *get_error(uint8_t code)
{
    switch (code)
    {
    case WIFI_REASON_UNSPECIFIED:
        return "WIFI_REASON_UNSPECIFIED";
    case WIFI_REASON_AUTH_EXPIRE:
        return "WIFI_REASON_AUTH_EXPIRE";
    case WIFI_REASON_AUTH_LEAVE:
        return "WIFI_REASON_AUTH_LEAVE";
    case WIFI_REASON_ASSOC_EXPIRE:
        return "WIFI_REASON_ASSOC_EXPIRE";
    case WIFI_REASON_ASSOC_TOOMANY:
        return "WIFI_REASON_ASSOC_TOOMANY";
    case WIFI_REASON_NOT_AUTHED:
        return "WIFI_REASON_NOT_AUTHED";
    case WIFI_REASON_NOT_ASSOCED:
        return "WIFI_REASON_NOT_ASSOCED";
    case WIFI_REASON_ASSOC_LEAVE:
        return "WIFI_REASON_ASSOC_LEAVE";
    case WIFI_REASON_ASSOC_NOT_AUTHED:
        return "WIFI_REASON_ASSOC_NOT_AUTHED";
    case WIFI_REASON_DISASSOC_PWRCAP_BAD:
        return "WIFI_REASON_DISASSOC_PWRCAP_BAD";
    case WIFI_REASON_DISASSOC_SUPCHAN_BAD:
        return "WIFI_REASON_DISASSOC_SUPCHAN_BAD";
    case WIFI_REASON_BSS_TRANSITION_DISASSOC:
        return "WIFI_REASON_BSS_TRANSITION_DISASSOC";
    case WIFI_REASON_IE_INVALID:
        return "WIFI_REASON_IE_INVALID";
    case WIFI_REASON_MIC_FAILURE:
        return "WIFI_REASON_MIC_FAILURE";
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
        return "WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT";
    case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT:
        return "WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT";
    case WIFI_REASON_IE_IN_4WAY_DIFFERS:
        return "WIFI_REASON_IE_IN_4WAY_DIFFERS";
    case WIFI_REASON_GROUP_CIPHER_INVALID:
        return "WIFI_REASON_GROUP_CIPHER_INVALID";
    case WIFI_REASON_PAIRWISE_CIPHER_INVALID:
        return "WIFI_REASON_PAIRWISE_CIPHER_INVALID";
    case WIFI_REASON_AKMP_INVALID:
        return "WIFI_REASON_AKMP_INVALID";
    case WIFI_REASON_UNSUPP_RSN_IE_VERSION:
        return "WIFI_REASON_UNSUPP_RSN_IE_VERSION";
    case WIFI_REASON_INVALID_RSN_IE_CAP:
        return "WIFI_REASON_INVALID_RSN_IE_CAP";
    case WIFI_REASON_802_1X_AUTH_FAILED:
        return "WIFI_REASON_802_1X_AUTH_FAILED";
    case WIFI_REASON_CIPHER_SUITE_REJECTED:
        return "WIFI_REASON_CIPHER_SUITE_REJECTED";
    case WIFI_REASON_INVALID_PMKID:
        return "WIFI_REASON_INVALID_PMKID";
    case WIFI_REASON_BEACON_TIMEOUT:
        return "WIFI_REASON_BEACON_TIMEOUT";
    case WIFI_REASON_NO_AP_FOUND:
        return "WIFI_REASON_NO_AP_FOUND";
    case WIFI_REASON_AUTH_FAIL:
        return "WIFI_REASON_AUTH_FAIL";
    case WIFI_REASON_ASSOC_FAIL:
        return "WIFI_REASON_ASSOC_FAIL";
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
        return "WIFI_REASON_HANDSHAKE_TIMEOUT";
    case WIFI_REASON_CONNECTION_FAIL:
        return "WIFI_REASON_CONNECTION_FAIL";
    case WIFI_REASON_AP_TSF_RESET:
        return "WIFI_REASON_AP_TSF_RESET";
    case WIFI_REASON_ROAMING:
        return "WIFI_REASON_ROAMING";
    default:
        return "WIFI_REASON_UNSPECIFIED";
    }
}

// broker that responds to whats happening between our idf and code
void event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch (event_id)
    {
    case SYSTEM_EVENT_STA_START:
        // inidicate the start of wifi
        ESP_LOGW("STA_EVENT", "CONNECTING...");
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_CONNECTED:
        ESP_LOGW("STA_EVENT", "CONNECTED...");
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED: // cannot declare a structure within a scope of switch statement ; so isolate it {}
    {
        wifi_event_sta_disconnected_t *wifi_event_sta_disconnected = event_data; // this pass argments for error when disconnect event is serviced
        if (wifi_event_sta_disconnected->reason == WIFI_REASON_ASSOC_LEAVE)
        {
            ESP_LOGW("STA_EVENT", "DISCONNECTED...");
            xEventGroupSetBits(wifi_events, DISCONNECTED_GOT_IP);
            break;
        }
        const char *err = get_error(wifi_event_sta_disconnected->reason);
        ESP_LOGW("STA_EVENT", "DISCONNECTED : %s", err);
        if((reconnect_count < 10) && (esp_wifi_connect() != ESP_OK))
        {
            reconnect_count++;
            ESP_LOGI("Reconnect_Count", "%d", reconnect_count);
        }
        xEventGroupSetBits(wifi_events, DISCONNECTED_GOT_IP); // wifi_events => BIT1 ; True
    }
    break;
    case IP_EVENT_STA_GOT_IP:
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGE("IP_EVENT", "STATION GOT IP.. ESP32-STA IP:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(wifi_events, CONNECTED_GOT_IP); // wifi_events => BIT0 ; True
    }
    break;
    case WIFI_EVENT_AP_START:
        ESP_LOGW("AP_EVENT", "AP STARTED...");
        start_dns_server();
        break;
    case WIFI_EVENT_AP_STACONNECTED: // SYSTEM_EVENT_AP_STADISCONNECTED
    {
        wifi_event_ap_staconnected_t *connected_event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGE("AP_EVENT", "station " MACSTR " joined, AID=%d", MAC2STR(connected_event->mac), connected_event->aid);
    }
    break;
    case WIFI_EVENT_AP_STADISCONNECTED: // SYSTEM_EVENT_AP_STADISCONNECTED
    {
        wifi_event_ap_stadisconnected_t *disconnected_event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGE("AP_EVENT", "station " MACSTR " left , AID=%d", MAC2STR(disconnected_event->mac), disconnected_event->aid);
    }
    break;
    case WIFI_EVENT_AP_STOP:
        ESP_LOGW("AP_EVENT", "AP STOPPED...");
        break;
    case SYSTEM_EVENT_AP_STAIPASSIGNED:
        ESP_LOGW("AP_EVENT", "IP ASSIGNED to connected device...");
        break;
    default:
        break;
    }
}

void wifi_init()
{
    ESP_ERROR_CHECK(esp_netif_init());                // or tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default()); // we are going to use an event loop

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, event_handler, NULL));
    // ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
}

esp_err_t wifi_connect_sta(const char *SSID, const char *PASS, int timeout)
{
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config)); // TCP/IP resource

    // create wifi event (connected/disconnected) // action -> state
    wifi_events = xEventGroupCreate();
    esp_netif = esp_netif_create_default_wifi_sta(); // just setting up the network interface

    // configuration structure
    wifi_config_t wifi_config;                                                             // handle
    memset(&wifi_config, 0, sizeof(wifi_config));                                          // or you can just pass null in handle
    strncpy((char *)wifi_config.sta.ssid, SSID, sizeof(wifi_config.sta.ssid) - 1);         // [31byte + 1-terminator] // only two types to choose fro (ap / sta)
    strncpy((char *)wifi_config.sta.password, PASS, sizeof(wifi_config.sta.password) - 1); // [31byte + 1-terminator] // only two types to choose fro (ap / sta)

    esp_netif_dhcpc_stop(esp_netif);
    esp_netif_ip_info_t ip_info;
    IP4_ADDR(&ip_info.ip, 192, 168, 1, 100);
    IP4_ADDR(&ip_info.gw, 192, 168, 1, 254);
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
    esp_netif_set_ip_info(esp_netif, &ip_info);

    esp_wifi_set_mode(WIFI_MODE_STA);                   // setting the mode of wifi (AP / STA)
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config); // pass the wifi_config ; if set as station
    esp_wifi_start();

    vTaskDelay(pdMS_TO_TICKS(10));

    Connect_Portal(); // open server with port:80
    http_server_sta_mode();

    EventBits_t result = xEventGroupWaitBits(wifi_events, CONNECTED_GOT_IP | DISCONNECTED_GOT_IP, true, false, pdMS_TO_TICKS(timeout));
    if (result == CONNECTED_GOT_IP)
    {
        return ESP_OK;
    }

    return ESP_FAIL;
}

void wifi_connect_ap(const char *SSID)
{
    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config_t));
    strncpy((char *)wifi_config.ap.ssid, SSID, sizeof(wifi_config.sta.ssid) - 1);
    wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    wifi_config.ap.max_connection = 4;
    wifi_config.ap.beacon_interval = 100;

    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
    esp_netif = esp_netif_create_default_wifi_ap();

    // for Manual static ip setup...
    Set_static_ip(esp_netif);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    vTaskDelay(pdMS_TO_TICKS(10));

    Connect_Portal(); // open server with port:53
    http_server_ap_mode();
}

void wifi_disconnect()
{
    ESP_LOGI("ESP_TAG", "**********DISCONNECTING*********");
    esp_wifi_disconnect();
    esp_wifi_stop();
    if (esp_netif)
    {
        esp_netif_destroy(esp_netif);
        esp_netif = NULL;
    }
    esp_wifi_deinit();
    ESP_LOGI("ESP_TAG", "***********DISCONNECTING COMPLETE*********");
}

static void Set_static_ip(esp_netif_t *netif)
{
    esp_netif_ip_info_t if_info = {0};
    esp_netif_dns_info_t dns_info = {0};

    ESP_ERROR_CHECK(esp_netif_dhcps_stop(netif));
    IP4_ADDR(&if_info.gw, 192, 168, 1, 1);
    IP4_ADDR(&if_info.ip, 192, 168, 1, 1);
    IP4_ADDR(&if_info.netmask, 255, 255, 255, 0);
    ESP_ERROR_CHECK(esp_netif_set_ip_info(netif, &if_info));
    ESP_ERROR_CHECK(esp_netif_get_ip_info(netif, &if_info));
    ESP_LOGE("AP_IP_TAG", "ESP32 IP:" IPSTR, IP2STR(&if_info.ip));
    ESP_LOGE("AP_IP_TAG", "ESP32 GW:" IPSTR, IP2STR(&if_info.gw));
    dns_info.ip.u_addr.ip4.addr = ESP_IP4TOADDR(1, 1, 1, 1); // set DNS IP : 1.1.1.1 => cloudFare / 8.8.8.8 => google
    ESP_ERROR_CHECK(esp_netif_set_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns_info));
    ESP_ERROR_CHECK(esp_netif_get_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns_info));
    ESP_LOGE("DNS_TAG", "ESP32 DNS_MAIN:" IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
    ESP_ERROR_CHECK(esp_netif_dhcps_start(netif));
}
