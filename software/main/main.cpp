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
    #include "esp_log.h"
    #include "esp_ota_ops.h"
}

#include "HttpServer.h"
#include "FilesystemHandler.h"
#include "StartupTest.h"
#include "WifiHandler.h"
#include "SettingsHandler.h"
#include "SwitchHandler.h"
#include "CurrentMeasurer.h"

__attribute__((unused)) static const char *TAG = "PowerSocket";


void hello_task(void *pvParameter)
{
    printf("Hello world!\n");
    while(true)
    {
        printf("HELLO!!, this is the amount of free heap: %u\t This is the minimum free heap ever: %u\n", xPortGetFreeHeapSize(), xPortGetMinimumEverFreeHeapSize());
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();
}


void cpp_main()
{
    const gpio_num_t relay_pins[] = {
        GPIO_NUM_27,
    };
    const gpio_num_t button_leds[] = {
        GPIO_NUM_26,
    };
    const gpio_num_t button_pins[] = {
        GPIO_NUM_25,
    };
    printf("Booted, now initialising tasks and subsystems!\n");
    printf("Compile info: GCC %u.%u.%u\t", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
    printf("Compile date: %s -- %s\n", __DATE__, __TIME__);

    //The initialisations of these systems ARE important!
    printf("Intialising settings handler and NVS system!\n");
    __attribute__((unused)) SettingsHandler *s_handler = SettingsHandler::get_instance();
    printf("Intialising switch handler!\n");
    __attribute__((unused)) SwitchHandler *switch_handler = SwitchHandler::get_instance(relay_pins, button_pins, button_leds, 1);


    printf("Initialising Wifi!\n");
    __attribute__((unused)) WifiHandler *wifi_h = WifiHandler::get_instance();
    printf("Initialised wifi!.\n");
    wifi_h->print_ap_settings();
    printf("Now initialising the filesystem.\n");
    FilesystemHandler *fs_handler = FilesystemHandler::get_instance(0x220000 /*Start address on flash*/,
                                 0x100000  /*Size*/,
                                 (char *)"/spiffs"      /*Mount point */);
    fs_handler->init_spiffs();
    xTaskCreate(&hello_task, "hello_task", 2048, NULL, 5, NULL);
    HttpServer httpd_server("80");
    httpd_server.start();
    HttpServer httpsd_server("443", true);
    httpsd_server.start();
    do_startup_test();

    const adc1_channel_t adc_channles[] = {
        ADC1_CHANNEL_0,
        ADC1_CHANNEL_1,
        ADC1_CHANNEL_2,
        ADC1_CHANNEL_3,
        ADC1_CHANNEL_4,
        ADC1_CHANNEL_5,
        ADC1_CHANNEL_6,
        ADC1_CHANNEL_7,
    };
    __attribute__((unused)) CurrentMeasurer *meas =  CurrentMeasurer::get_instance(adc_channles, sizeof(adc_channles) / sizeof(adc_channles[0]) );
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
