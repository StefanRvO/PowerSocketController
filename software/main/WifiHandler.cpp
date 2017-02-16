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
#undef ESP_ERROR_CHECK
#define ESP_ERROR_CHECK(x)   do { esp_err_t rc = (x); if (rc != ESP_OK) { ESP_LOGE("err", "esp_err_t = %d", rc); assert(0 && #x);} } while(0);

#include "WifiHandler.h"

static const char *TAG = "WifiHandler";

//This bit is cleared if we are connected and set if not connected
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
    ESP_ERROR_CHECK( this->s_handler->nvs_get("STA_SSID", (char *)wifi_config.sta.ssid, &len_ssid));
    ESP_ERROR_CHECK( this->s_handler->nvs_get("STA_PASSWORD", (char *)wifi_config.sta.password, &len_passwd) );

    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    //ESP_ERROR_CHECK( esp_wifi_set_auto_connect(true) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
}

void WifiHandler::update_ap_config()
{
    //Set up wifi access point configuration
    wifi_config_t ap_config;
    size_t ssid_len = sizeof(ap_config.ap.ssid);
    ESP_ERROR_CHECK( this->s_handler->nvs_get("AP_SSID", (char *)ap_config.ap.ssid,
        &ssid_len));
    size_t pass_len = sizeof(ap_config.ap.password);
    ESP_ERROR_CHECK( this->s_handler->nvs_get("AP_PASS", (char *)ap_config.ap.password,
        &pass_len));

    ap_config.ap.ssid_len = 0;  //ONLY USED IF NOT 0 teminated.
    ESP_ERROR_CHECK( this->s_handler->nvs_get("AP_AUTH", (uint32_t *)(&ap_config.ap.authmode)));

    ap_config.ap.ssid_hidden = 0;
    ap_config.ap.max_connection = 20;
    ap_config.ap.beacon_interval = 100;
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_AP, &ap_config));
}

void WifiHandler::update_wifi_mode()
{
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_APSTA) ); //Configure as both accesspoint and station
}

void WifiHandler::print_ap_settings()
{
    //Get current settings for our accesspoint
    tcpip_adapter_ip_info_t ip_info;
    ESP_ERROR_CHECK (tcpip_adapter_get_ip_info (TCPIP_ADAPTER_IF_AP , &ip_info));
    printf("Now printing the AP setup\n");
    printf("IP: ");
    for(uint8_t i = 0; i < sizeof(ip4_addr_t); i++)
    {
        printf("%hu.", *(((uint8_t *)&ip_info.ip) + i) );
    }
    printf("\nnetmask: ");
    for(uint8_t i = 0; i < sizeof(ip4_addr_t); i++)
    {
        printf("%hu.", *(((uint8_t *)&ip_info.netmask) + i));
    }
    printf("\ngateway: ");
    for(uint8_t i = 0; i < sizeof(ip4_addr_t); i++)
    {
        printf("%hu.", *(((uint8_t *)&ip_info.gw) + i));
    }
    printf("\n");
}

void WifiHandler::initialise_wifi(void)
{
    tcpip_adapter_init();
    WifiHandler::wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    this->update_wifi_mode();
    this->update_ap_config();
    this->update_station_config();
    ESP_ERROR_CHECK( esp_wifi_start() );
    //Start reconnect thread (how much stack do we need??)
    xTaskCreate(WifiHandler::reconnect_thread, "Wifi_Reconnect", 1024, this, 10, NULL);
}


esp_err_t WifiHandler::event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupClearBits(WifiHandler::wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
        //esp_wifi_connect();
        xEventGroupSetBits(WifiHandler::wifi_event_group, CONNECTED_BIT);
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

void WifiHandler::reconnect_thread(void *PvParameters)
{
    //Task for handling reconnection to AP. Whan recieving a disconnect
    WifiHandler *handler = (WifiHandler *)PvParameters;
    while(true)
    {
        EventBits_t bits = xEventGroupWaitBits(handler->wifi_event_group, BIT0, pdFALSE,pdTRUE, portMAX_DELAY);

        if ((bits | BIT0))
        {   //Try to reconnect in 10 seconds after disconnect
            vTaskDelay(10000 / portTICK_PERIOD_MS);
            esp_wifi_connect();
        }

    }

}
