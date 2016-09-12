/* A very basic C++ example, really just proof of concept for C++

   This sample code is in the public domain.
 */
 extern "C"
 {
    #include "FreeRTOS.h"
    #include "task.h"
    #include <esp/uart.h>
    #include "rboot-api.h"
    #include "ota-tftp.h"
}

#include "SwitchServer.h"
#include "SettingsReader.h"
#include "WifiHandler.h"
#include "HttpdServer.h"

#define os_printf printf

#define delay_ms(ms) vTaskDelay((ms) / portTICK_RATE_MS)

void Status_Print(void *pvParameters)
{
    while(true)
    {
        delay_ms(1000);
        printf("free heap: %d\n", sdk_system_get_free_heap_size());
    }
}

extern "C" void user_init(void)
{
    uart_set_baud(0, 115200);
    printf("SDK version:%s\n", sdk_system_get_sdk_version());
    rboot_config conf = rboot_get_config();
    printf("\r\n\r\nOTA Basic demo.\r\nCurrently running on flash slot %d / %d.\r\n\r\n",
           conf.current_rom, conf.count);

    printf("Image addresses in flash:\r\n");
    for(int i = 0; i <conf.count; i++) {
        printf("%c%d: offset 0x%08x\r\n", i == conf.current_rom ? '*':' ', i, conf.roms[i]);
    }

    SettingsReader::init();
    WifiHandler::init();
    ota_tftp_init_server(TFTP_PORT);
    SwitchServer::start_server(4999);
    HttpdServer(80);
    xTaskCreate(Status_Print, (signed char *)"Bind Task", 1000, NULL, 10, NULL);
}
