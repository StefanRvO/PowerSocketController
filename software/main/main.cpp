
extern "C"
{
    #include <stdio.h>
    #include "freertos/FreeRTOS.h"
    #include "freertos/task.h"
    #include "esp_system.h"
    #include "esp_log.h"
    #include "esp_ota_ops.h"
    #include "apps/sntp/sntp.h"

}

#include "HttpServer.h"
#include "FilesystemHandler.h"
#include "WifiHandler.h"
#include "SettingsHandler.h"
#include "SwitchHandler.h"
#include "TimeKeeper.h"
#include "mDNSServer.h"
#include "PCF8574_Handler.h"

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
        vTaskDelay(1000 / portTICK_PERIOD_MS);
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
    printf("Intialising settings handler and NVS system!\n");
    __attribute__((unused)) SettingsHandler *s_handler = SettingsHandler::get_instance();
    printf("Initialising PCF8574 devices");
    PCF8574 PCF8574_devices[2];
    PCF8574 &button_control = PCF8574_devices[0];
    button_control.address = 0x38;
    button_control.interrupt_pin = (gpio_num_t)0;
    PCF8574 &relay_control = PCF8574_devices[2];
    relay_control.address = 0x39;
    relay_control.interrupt_pin = GPIO_NUM_23;

    PCF8574_Handler::get_instance(GPIO_NUM_21, GPIO_NUM_19, PCF8574_devices, 2);

    printf("Intialising switch handler!\n");
    const gpio_num_t button_leds[] = {
        GPIO_NUM_19,
        GPIO_NUM_5,
        GPIO_NUM_16,
    };
    PCF8574_pin relay_pins[3];
    PCF8574_pin button_pins[3];
    PCF8574_pin relay_voltage_pin;
    relay_voltage_pin.address = 0x39;
    relay_voltage_pin.pin_num = 0;
    relay_pins[0].address = 0x39;
    relay_pins[1].address = 0x39;
    relay_pins[2].address = 0x39;
    relay_pins[0].pin_num = 3;
    relay_pins[1].pin_num = 2;
    relay_pins[2].pin_num = 1;

    button_pins[0].address = 0x38;
    button_pins[1].address = 0x38;
    button_pins[2].address = 0x38;
    button_pins[0].pin_num = 0;
    button_pins[1].pin_num = 1;
    button_pins[2].pin_num = 2;

    SwitchHandler::get_instance(relay_pins, &relay_voltage_pin, button_pins, button_leds, 3);


    printf("Initialising Wifi!\n");
    __attribute__((unused)) WifiHandler *wifi_h = WifiHandler::get_instance();
    printf("Initialised wifi!.\n");
    wifi_h->print_ap_settings();
    xTaskCreate(&hello_task, "hello_task", 2048, NULL, 5, NULL);

    initialize_sntp();

    //Keep time.
    TimeKeeper *t_keeper = TimeKeeper::get_instance();
    printf("Startup done\n");
    HttpServer httpd_server("80");
    httpd_server.start();
    //HttpServer httpsd_server("443", true);
    //httpsd_server.start();
    __attribute__((unused)) mDNSServer *mdns_server = mDNSServer::get_instance();

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
