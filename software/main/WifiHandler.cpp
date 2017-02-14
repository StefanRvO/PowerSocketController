extern "C"
{
    #include <string.h>
    #include "freertos/FreeRTOS.h"

    #include "freertos/event_groups.h"
    #include "freertos/task.h"

    #include "esp_wifi.h"
    #include "esp_event_loop.h"
    #include "esp_log.h"
}

#include "WifiHandler.h"

static const char *TAG = "WifiHandler";

/* FreeRTOS event group to signal when we are connected & ready to make a request */
const int CONNECTED_BIT = BIT0;

EventGroupHandle_t WifiHandler::wifi_event_group;
WifiHandler *WifiHandler::instance = nullptr;


WifiHandler *WifiHandler::get_instance()
{
    if(WifiHandler::instance == nullptr)
    {
        WifiHandler::instance = new WifiHandler();
    }
    return WifiHandler::instance;
}


WifiHandler::WifiHandler()
: s_handler(SettingsHandler::get_instance())
{
    this->initialise_wifi();
}

WifiHandler::~WifiHandler() {}

void WifiHandler::update_station_config()
{
    //Set up wifi station configuration.

    wifi_config_t wifi_config = {
        .sta = {
            "",
            "",
            false,
            "",
        },
    };

    //Retrieve setting from NVS
    size_t len_passwd = 64;
    size_t len_ssid = 32;
    esp_err_t err = s_handler->nvs_get("STA_SSID", (char *)wifi_config.sta.ssid, &len_ssid);
    printf("%d\n", err);
    err = s_handler->nvs_get("STA_PASSWORD", (char *)wifi_config.sta.password, &len_passwd);
    printf("%d\n", err);

    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_set_auto_connect(true) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
}

void WifiHandler::initialise_wifi(void)
{
    tcpip_adapter_init();
    WifiHandler::wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );

    //Set up wifi access point configuration
    wifi_config_t ap_config;
    uint8_t ap_ssid[32] = "ESP32_AP\0";
    uint8_t ap_pass[64] = "\0";
    memcpy(ap_config.ap.ssid, ap_ssid, 32);
    memcpy(ap_config.ap.password, ap_pass, 64);
    ap_config.ap.ssid_len = 0;  //ONLY USED IF NOT 0 teminated.
    ap_config.ap.authmode = WIFI_AUTH_OPEN;
    ap_config.ap.ssid_hidden = 0;
    ap_config.ap.max_connection = 20;
    ap_config.ap.beacon_interval = 100;

    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_APSTA) ); //Configure as both accesspoint and station
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    this->update_station_config();
    ESP_ERROR_CHECK( esp_wifi_start() );
}


esp_err_t WifiHandler::event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(WifiHandler::wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
        //esp_wifi_connect();
        xEventGroupClearBits(WifiHandler::wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_AP_STACONNECTED:
        printf("station connected to access point. Now listing connected stations!\n");
        wifi_sta_list_t sta_list;
        ESP_ERROR_CHECK( esp_wifi_ap_get_sta_list(&sta_list));
        for(int i = 0; i < sta_list.num; i++)
        {
            //Print the mac address of the connected station
            wifi_sta_info_t &sta = sta_list.sta[i];
            printf("Station %d MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", i,
                sta.mac[0], sta.mac[1], sta.mac[2], sta.mac[3], sta.mac[4], sta.mac[5]);
        }

    default:
        break;
    }
    return ESP_OK;
}
