
extern "C"
{
    #include <stdio.h>
    #include "freertos/FreeRTOS.h"
    #include "freertos/task.h"
    #include "esp_system.h"
    #include "esp_log.h"
    #include "apps/sntp/sntp.h"
}

#include "HttpServer.h"
#include "FilesystemHandler.h"
#include "WifiHandler.h"
#include "SettingsHandler.h"
#include "TimeKeeper.h"
#include "mDNSServer.h"
#include "Hardware_Initialiser.h"

__attribute__((unused)) static const char *TAG = "PowerSocket";


static void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, (char *)"pool.ntp.org");
    sntp_init();
}


void hello_task(void *pvParameter)
{
    printf("Hello world!\n");
    while(true)
    {
        printf("HELLO!!, this is the amount of free heap: %u\t This is the minimum free heap ever: %u\n", xPortGetFreeHeapSize(), xPortGetMinimumEverFreeHeapSize());
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();
}


void cpp_main()
{

    printf("Booted, now initialising tasks and subsystems!\n");
    printf("Compile info: GCC %u.%u.%u\t", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
    printf("Compile date: %s -- %s\n", __DATE__, __TIME__);

    //The initialisations of these systems ARE important!
    //This initialises esp32 systems which should not be dependent on external hardware.
    printf("Intialising settings handler and NVS system!\n");
    __attribute__((unused)) SettingsHandler *s_handler = SettingsHandler::get_instance();
    xTaskCreate(&hello_task, "hello_task", 2048, NULL, 5, NULL);
    printf("Initialising Wifi!\n");
    __attribute__((unused)) WifiHandler *wifi_h = WifiHandler::get_instance();
    printf("Initialised wifi!.\n");
    wifi_h->print_ap_settings();
    initialize_sntp();
    mDNSServer::get_instance();
    TimeKeeper *t_keeper = TimeKeeper::get_instance();
    //Initialise hardware dependent on the PCB version
    if(Hardware_Initialiser::initialise_hardware())
    {
        printf("Hardware_intialisation failed. This will probably cause the HTTP server to crash too\n");
    }
    printf("Startup done\n");
    HttpServer httpd_server("80");
    httpd_server.start();

    //Enter a inifite loop which performs tasks which should happen very unfrequent
    while(1)
    {
        t_keeper->do_update();
        vTaskDelay((1000 * 60 * 60 * 24) / portTICK_PERIOD_MS); //Once per day
        printf("Time is %f\n", t_keeper->get_uptime());

    }
}

extern "C"
{
    void app_main()
    {
        cpp_main();
    }
}
