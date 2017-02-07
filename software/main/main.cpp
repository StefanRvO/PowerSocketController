/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
extern "C"
{
    #include <stdio.h>
    #include "freertos/FreeRTOS.h"
    #include "freertos/task.h"
    #include "esp_system.h"
    #include "nvs_flash.h"
    #include "esp_wifi.h"
    #include "esp_event_loop.h"
    #include "esp_log.h"
    #include "esp_ota_ops.h"
    #include "esp_partition.h"
    #include "freertos/event_groups.h"
}
#include "HttpServer.h"
#include "SwitchServer.h"
#include "FilesystemHandler.h"

#define EXAMPLE_WIFI_SSID "Dommedagsdomicilet"
#define EXAMPLE_WIFI_PASS ""

static const char *TAG = "PowerSocket";


/* FreeRTOS event group to signal when we are connected & ready to make a request */

static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}

static void initialise_wifi(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    wifi_config_t wifi_config = {
        .sta = {
            EXAMPLE_WIFI_SSID,
            EXAMPLE_WIFI_PASS,
        },
    };
    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}


void hello_task(void *pvParameter)
{
    printf("Hello world!\n");
    while(true)
    {
        printf("HELLO!!\n");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();
}


extern "C"
{
    void app_main()
    {
        printf("Booted, now initialising tasks and subsystems!\n");
        nvs_flash_init();
        xTaskCreate(&hello_task, "hello_task", 2048, NULL, 5, NULL);
        printf("Initialising Wifi!\n");
        initialise_wifi();
        printf("Initialised wifi!.\b");
        printf("Now initialising the filesystem.\n");
        SwitchServer::start_server(4500);
        HttpServer httpd_server(":80");
        httpd_server.start();
        printf("Startup done. Returning from main!\n");
        FilesystemHandler fs_handler(0x310000 /*Start address on flash*/,
                                     0x80000  /*Size*/,
                                     "/"      /*Mount point */);

    }
}
