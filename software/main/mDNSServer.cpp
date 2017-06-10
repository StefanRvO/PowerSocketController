#include "mDNSServer.h"
#include "esp_log.h"
#include "WifiHandler.h"
#include <cstring>
static const char *TAG = "mdns";

static const char * arduTxtData[3] = {
    "board_rev=esp32_0.1",
    "sockets=3",
    "api=v1",
};

mdns_server_t *mDNSServer::server_sta = nullptr;
mdns_server_t *mDNSServer::server_ap = nullptr;

mDNSServer *mDNSServer::instance = nullptr;

mDNSServer *mDNSServer::get_instance()
{
    if(mDNSServer::instance == nullptr)
    {
        mDNSServer::instance = new mDNSServer();
    }
    return mDNSServer::instance;
}

mDNSServer::mDNSServer()
{
    xTaskCreatePinnedToCore(mDNSServer::mdns_monitor, "mDNSmonitor", 2048, this, 10, NULL, 0);

};


void mDNSServer::mdns_monitor(__attribute__((unused)) void *PvParameters)
{
    esp_err_t err;
    tcpip_adapter_ip_info_t last_sta_info;
    tcpip_adapter_ip_info_t last_ap_info;

    while(1)
    {
        tcpip_adapter_ip_info_t cur_info;
        ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &cur_info));
        //reinit if address changed and we are connected
        bool connected = !(xEventGroupGetBits( WifiHandler::wifi_event_group ) & WifiHandler::CONNECTED_BIT);
        bool sta_changed = memcmp(&cur_info, &last_sta_info, sizeof(tcpip_adapter_ip_info_t)) != 0;
        last_sta_info = cur_info;
        ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &cur_info));
        bool ap_changed = memcmp(&cur_info, &last_ap_info, sizeof(tcpip_adapter_ip_info_t)) != 0;
        last_ap_info = cur_info;

        if( sta_changed != 0 && connected && mDNSServer::server_sta)
        {
             mdns_free(mDNSServer::server_sta);
             mDNSServer::server_sta = nullptr;
             ESP_LOGI(TAG, "Stopped MDNS on STA interface!");
        }
        if((ap_changed || true )&& mDNSServer::server_ap)
        {
             mdns_free(mDNSServer::server_ap);
             mDNSServer::server_ap = nullptr;
             ESP_LOGI(TAG, "Stopped MDNS on AP interface!");
        }
        if(connected && mDNSServer::server_sta == nullptr)
        {
            err = mdns_init(TCPIP_ADAPTER_IF_STA, &mDNSServer::server_sta);
            if (err) {
                ESP_LOGE(TAG, "Failed starting MDNS STA: %d", err);
            }
            ESP_LOGI(TAG, "Started MDNS on STA interface!");
        }
        if(mDNSServer::server_ap == nullptr)
        {
            err = mdns_init(TCPIP_ADAPTER_IF_AP, &mDNSServer::server_ap);
            if (err) {
                ESP_LOGE(TAG, "Failed starting MDNS AP: %d", err);
            }
            ESP_LOGI(TAG, "Started MDNS on AP interface!");
        }
        mdns_server_t * servers[] = {mDNSServer::server_sta, mDNSServer::server_ap};

        for(uint8_t i = 0; i < sizeof(servers) / sizeof(servers[0]); i++ )
        {
            if(servers[i] == nullptr) continue;
            ESP_ERROR_CHECK( mdns_set_hostname(servers[i], "powersocket") );
            ESP_ERROR_CHECK( mdns_set_instance(servers[i], "test2") );

            ESP_ERROR_CHECK( mdns_service_add(servers[i], "_powersocket_http", "_tcp", 80) );
            ESP_ERROR_CHECK( mdns_service_txt_set(servers[i], "_powersocket_http", "_tcp", 3, arduTxtData) );
            ESP_ERROR_CHECK( mdns_service_add(servers[i], "_powersocket_https", "_tcp", 443) );
            ESP_ERROR_CHECK( mdns_service_txt_set(servers[i], "_powersocket_https", "_tcp", 3, arduTxtData) );
        }
        vTaskDelay((10000) / portTICK_PERIOD_MS); //every 5 seconds
    }

};
