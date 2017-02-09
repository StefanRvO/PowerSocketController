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

    //Set up wifi station configuration.
    wifi_config_t wifi_config = {
        .sta = {
            EXAMPLE_WIFI_SSID,
            EXAMPLE_WIFI_PASS,
            false,
            "",
        },
    };
    //Set up wifi access point configuration
    wifi_config_t ap_config;
    uint8_t ap_ssid[32] = "ESP32_AP";
    uint8_t ap_pass[64] = "";
    memcpy(ap_config.ap.ssid, ap_ssid, 32);
    memcpy(ap_config.ap.password, ap_pass, 64);
    ap_config.ap.ssid_len = 0;  //ONLY USED IF NOT 0 teminated.
    ap_config.ap.authmode = WIFI_AUTH_OPEN;
    ap_config.ap.ssid_hidden = 0;
    ap_config.ap.max_connection = 4;
    ap_config.ap.beacon_interval = 100;

    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_APSTA) ); //Configure as both accesspoint and station
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK( esp_wifi_start() );
}


void hello_task(void *pvParameter)
{
    printf("Hello world!\n");
    while(true)
    {
        printf("HELLO!!, this is the amount of free heap: %u\n", xPortGetFreeHeapSize());
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();
}


void cpp_main()
{
    printf("Booted, now initialising tasks and subsystems!\n");
    nvs_flash_init();
    xTaskCreate(&hello_task, "hello_task", 2048, NULL, 5, NULL);
    printf("Initialising Wifi!\n");
    initialise_wifi();
    printf("Initialised wifi!.\n");
    printf("Now initialising the filesystem.\n");
    FilesystemHandler *fs_handler = FilesystemHandler::get_instance(0x210000 /*Start address on flash*/,
                                 0x100000  /*Size*/,
                                 (char *)"/spiffs"      /*Mount point */);
    fs_handler->init_spiffs();
    SwitchServer::start_server(4500);
    HttpServer httpd_server("80");
    httpd_server.start();
    HttpServer httpsd_server("443", true);
    httpsd_server.start();

    #define CHUNK 150 /* read 1024 bytes at a time */
    char buf[CHUNK];
    int nread;

    //Read using spiffs api
    printf("Trying to read using SPIFFS API!\n");
    spiffs_file f = SPIFFS_open(&fs_handler->fs, "/README", SPIFFS_O_RDONLY, 0);

    printf("%d\n", (size_t)f);
    if (f) {
        while ((nread = SPIFFS_read(&fs_handler->fs, f, buf, sizeof buf)) > 0)
        {
            printf("%.*s", nread, buf);
        }
        printf("%d\n", nread);
        SPIFFS_close(&fs_handler->fs, f);
        printf("\n");
    }
    printf("Now trying to read using VFS API!\n");

    FILE *file;
    file = fopen("/spiffs/README", "rw");
    printf("%d\n", (size_t)f);
    if (file) {
        while (fgets(buf, 50, file) != nullptr)
        {
            printf("%s", buf);
        }
        fclose(file);
        printf("\n");
    }
    printf("Now trying to list the files in the spiffs partition!\n");
    printf("We first try using SPIFFS calls directly.\n");

    spiffs_DIR dir;
	struct spiffs_dirent dirEnt;
	const char rootPath[] = "/";


	if (SPIFFS_opendir(&fs_handler->fs, rootPath, &dir) == NULL) {
		printf("Unable to open %s dir", rootPath);
		return;
	}
	while(SPIFFS_readdir(&dir, &dirEnt) != NULL) {
		int len = strlen((char *)dirEnt.name);
		// Skip files that end with "/."
		if (len>=2 && strcmp((char *)(dirEnt.name + len -2), "/.") == 0) {
			continue;
		}
        printf("%s\n", dirEnt.name);
    }
    SPIFFS_closedir(&dir);


    printf("We now try through the filesystem.\n");
    DIR *midir;
    struct dirent* info_archivo;
    if ((midir = opendir("/spiffs/")) == NULL)
    {
        perror("Error in opendir\n");
    }
    else
    {
        while ((info_archivo = readdir(midir)) != 0)
            printf ("%s \n", info_archivo->d_name);
    }
    closedir(midir);

    printf("We see if stat is working by accessing a file which we know is there.\n");
    struct stat stat_buf;
    printf("Stat result:%d\n",stat("/spiffs/README", &stat_buf));
    printf("Startup done. Suspending main task\n");
    vTaskSuspend( NULL );
}

extern "C"
{
    void app_main()
    {
        cpp_main();
    }
}
