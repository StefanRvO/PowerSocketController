#pragma once
extern "C"
{
    #include "freertos/FreeRTOS.h"
    #include "esp_system.h"
    #include "esp_event.h"
    #include "esp_event_loop.h"
    #include "freertos/event_groups.h"

}

#include "SettingsHandler.h"

class WifiHandler;

class WifiHandler
{
    public:
        static WifiHandler *get_instance();
        ~WifiHandler();
        void initialise_wifi();
        static esp_err_t event_handler(void *ctx, system_event_t *event);
        static EventGroupHandle_t wifi_event_group;
        void update_station_config();
        SettingsHandler *s_handler = nullptr;
    private:
        static WifiHandler *instance;
        WifiHandler();
};
