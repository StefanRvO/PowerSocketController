#include "WifiHandler.h"
#include "SettingsReader.h"
extern "C"
{
    #include "espressif/esp_common.h"
}

void WifiHandler::init()
{
    struct sdk_station_config config = {
        "Troldkaervej 24",
        "\000000000000000000000",
    };
    SettingsReader::get_wifi_ssid((char *)config.ssid, 20);
    SettingsReader::get_wifi_password((char *)config.password, 20);

    config.bssid_set = 0;
    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_set_config(&config);
    sdk_wifi_station_set_auto_connect(true);

}

void WifiHandler::reconnect()
{
    sdk_wifi_station_disconnect();
    sdk_wifi_station_connect();

}

bool WifiHandler::is_connected()
{
    uint8_t status = sdk_wifi_station_get_connect_status();
    return status == STATION_GOT_IP;
}
