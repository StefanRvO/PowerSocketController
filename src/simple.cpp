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
}

#include "Connection.h"
#include "Message.h"
#include "MessageParser.h"

#define delay_ms(ms) vTaskDelay((ms) / portTICK_RATE_MS)

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

extern "C" void wifi_reconnect()
{
    sdk_wifi_station_disconnect();
    sdk_wifi_station_connect();
}


void setup_wifi()
{
    struct sdk_station_config config = {
        WIFI_SSID,
        WIFI_PASS,
    };
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
    setup_wifi();

    xTaskCreate(Bind_Task, (signed char *)"Bind Task", 1000, NULL, 10, NULL);
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
