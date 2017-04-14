#include "mDNSServer.h"
#include "esp_log.h"

static const char *TAG = "mdns";

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
    esp_err_t err = mdns_init(TCPIP_ADAPTER_IF_STA, &this->server);
    if (err) {
        ESP_LOGE(TAG, "Failed starting MDNS: %u", err);
    }
    ESP_LOGI(TAG, "Started MDNS!");

    ESP_ERROR_CHECK( mdns_set_hostname(this->server, "test1") );
    ESP_ERROR_CHECK( mdns_set_instance(this->server, "test2") );
    const char * arduTxtData[3] = {
        "board_rev=esp32_0.1",
        "sockets=3",
        "api=v1",
    };
    ESP_ERROR_CHECK( mdns_service_add(this->server, "_powersocket_http", "_tcp", 80) );
    ESP_ERROR_CHECK( mdns_service_txt_set(this->server, "_powersocket_http", "_tcp", 3, arduTxtData) );
    ESP_ERROR_CHECK( mdns_service_add(this->server, "_powersocket_https", "_tcp", 443) );
    ESP_ERROR_CHECK( mdns_service_txt_set(this->server, "_powersocket_https", "_tcp", 3, arduTxtData) );

};
