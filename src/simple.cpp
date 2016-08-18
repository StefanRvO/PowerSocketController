/* A very basic C++ example, really just proof of concept for C++

   This sample code is in the public domain.
 */
 extern "C"
 {
#include "espressif/esp_common.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "lwip/tcp.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/tcp_impl.h"
#include "lwip/init.h"
#include "lwip/timers.h"
#include "lwip/ip_addr.h"
#include <esp/uart.h>
#include "ssid_config.h"
}

#include "Connection.h"
#include "Message.h"
#include "MessageParser.h"

#define delay_ms(ms) vTaskDelay((ms) / portTICK_RATE_MS)

tcp_pcb *listner_socket = nullptr;
void Connection_Handler(void *PvParameters);


err_t new_connection(void *arg, tcp_pcb *newpcb, err_t err)
{
    Connection *new_con = new Connection(newpcb);
    xTaskCreate(Connection_Handler, (signed char *)"Conn Handler", 250, new_con, 10, NULL);

    tcp_accepted(listner_socket);
    return err;
}

bool is_connected()
{
    uint8_t status = sdk_wifi_station_get_connect_status();
    return status == STATION_GOT_IP;
}

void TCP_Timer(void *pvParameters)
{
    while(true)
    {
        tcp_tmr();
        delay_ms(TCP_TMR_INTERVAL);
    }
}


void Bind_Task(void *pvParameters)
{
        delay_ms(0);
        tcp_pcb *listner_socket = tcp_new();
        ip_addr bind_ip;
        bind_ip.addr = inet_addr("192.168.0.102");

        tcp_bind(listner_socket, &bind_ip, 5000);
        listner_socket = tcp_listen(listner_socket);
        printf("Bound to port 5000");
        tcp_accept(listner_socket, new_connection);
        while(true)
        {
            delay_ms(100);
            printf("%d\n", sdk_system_get_free_heap_size());
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

    lwip_init();
    xTaskCreate(Bind_Task, (signed char *)"Bind Task", 1000, NULL, 10, NULL);
    xTaskCreate(TCP_Timer, (signed char *)"TCP_Timer", 1000, NULL, 10, NULL);
}

void Connection_Handler(void *PvParameters)
{
    Connection *connection = (Connection *)PvParameters;
    MessageParser parser(connection);
    printf("Entered connection handler\n");
    char recv_char;
    while(connection->is_connected())
    {
        delay_ms(0);
        Message *msg = Message::receive_message(connection);
        if(msg == nullptr)
        {
            connection->close();
            break;
        }
        parser.parse_message(msg);
        delete msg;
    }
    printf("Connection was closed");
    delete connection;
    vTaskDelete( NULL );
}
