/* A very basic C++ example, really just proof of concept for C++

   This sample code is in the public domain.
 */
 extern "C"
 {
    #include "espressif/esp_common.h"
    #include "FreeRTOS.h"
    #include "task.h"
    #include "queue.h"

    #include <esp/uart.h>
    #include "ssid_config.h"
    #include "rboot-api.h"
    #include "ota-tftp.h"

}

#include "Connection.h"
#include "Message.h"
#include "MessageParser.h"
#include "SettingsReader.h"

#define delay_ms(ms) vTaskDelay((ms) / portTICK_RATE_MS)

#define TFTP_IMAGE_SERVER "192.168.1.23"
#define TFTP_IMAGE_FILENAME1 "firmware1.bin"
#define TFTP_IMAGE_FILENAME2 "firmware2.bin"


void Connection_Handler(void *PvParameters);

bool is_connected()
{
    uint8_t status = sdk_wifi_station_get_connect_status();
    return status == STATION_GOT_IP;
}


void Bind_Task(void *pvParameters)
{
        delay_ms(0);
        uint16_t port = 4999;
        if(!Connection::bind_to(port))
        {
            printf("Listen on port %u failed!\n", (unsigned int)port);
        }
        else printf("Listening on port %u\n", (unsigned int)port);
        while(true)
        {
            delay_ms(1);
            printf("%d\n", sdk_system_get_free_heap_size());
            Connection *new_connection = Connection::get_next_incomming();
            if(new_connection == nullptr) continue;
            xTaskCreate(Connection_Handler, (signed char *)"Con_handler", 1000, new_connection, 10, NULL);
        }
}

void Status_Print(void *pvParameters)
{

    while(true)
    {
        delay_ms(1000);
        printf("free heap: %d\n", sdk_system_get_free_heap_size());
    }

}
void ota_task(void *pvParameters)
{
    while(true)
    {
        rboot_config conf = rboot_get_config();
        int slot = (conf.current_rom + 1) % conf.count;
        if(slot == conf.current_rom) {
            printf("FATAL ERROR: Only one OTA slot is configured!\n");
            while(1) {}
        }
        int res = ota_tftp_download(TFTP_IMAGE_SERVER, TFTP_PORT+1, TFTP_IMAGE_FILENAME2, 1000, slot, NULL);
        vTaskDelay(5000 / portTICK_RATE_MS);
    }


}

extern "C" void wifi_reconnect()
{
    sdk_wifi_station_disconnect();
    sdk_wifi_station_connect();
}


void setup_wifi()
{
    struct sdk_station_config config = {
        "Troldkaervej 24",
        "\000000000000000000000",
    };

    SettingsReader::readfile(WIFI_SSID_FILE, (char *)config.ssid, 20);
    SettingsReader::readfile(WIFI_SSID_FILE, (char *)config.password, 20);

    config.bssid_set = 0;
    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_set_config(&config);
    sdk_wifi_station_set_auto_connect(true);
    //wifi_reconnect();
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

    setup_wifi();
    ota_tftp_init_server(TFTP_PORT);
    xTaskCreate(Bind_Task, (signed char *)"Bind Task", 1000, NULL, 10, NULL);
    xTaskCreate(Status_Print, (signed char *)"Bind Task", 1000, NULL, 10, NULL);

}

void Connection_Handler(void *PvParameters)
{
    Connection *connection = (Connection *)PvParameters;
    MessageParser parser(connection);
    char *clientip = new char[20];
    connection->get_client_ip(clientip);
    uint16_t port = connection->get_client_port();
    printf("Entered connection handler for client with IP: %s and port %u\n", clientip, (unsigned int)port);

    delete[] clientip;
    while(connection->is_connected())
    {
        delay_ms(0);
        Message *msg = Message::receive_message(connection);
        if(msg == nullptr)
        {
            connection->close_connection();
            break;
        }
        parser.parse_message(msg);
        delete msg;
    }
    printf("Connection was closed\n");
    delete connection;
    vTaskDelete( NULL );
}
