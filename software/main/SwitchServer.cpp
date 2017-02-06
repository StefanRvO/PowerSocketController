#include "SwitchServer.h"
#include "Connection.h"
#include "Message.h"

extern "C"
{
    #include "freertos/FreeRTOS.h"
    #include "freertos/task.h"
}

uint16_t SwitchServer::port = 0;

void SwitchServer::start_server(uint16_t _port)
{
    SwitchServer::port = _port;
    xTaskCreate(SwitchServer::Bind_Task, "Bind Task", 4000, NULL, 10, NULL);
}

void SwitchServer::Connection_Handler(void *PvParameters)
{
    Connection *connection = (Connection *)PvParameters;
    //MessageParser parser(connection);
    char *clientip = new char[20];
    connection->get_client_ip(clientip);
    uint16_t port = connection->get_client_port();
    printf("Entered connection handler for client with IP: %s and port %u\n", clientip, (unsigned int)port);

    delete[] clientip;
    while(connection->is_connected())
    {
        vTaskDelay(0);
        Message *msg = Message::receive_message(connection);
        if(msg == nullptr)
        {
            connection->close_connection();
            break;
        }
        printf("Recieved message of type %d\n", msg->get_type());
        //parser.parse_message(msg);
        delete msg;
    }
    printf("Connection was closed\n");
    delete connection;
    vTaskDelete( NULL );
}


void SwitchServer::Bind_Task(void *pvParameters)
{
        if(!Connection::bind_to(SwitchServer::port))
        {
            printf("Listen on port %u failed!\n", (unsigned int)SwitchServer::port);
            printf("Bailing out, this should not happen!\n");
            printf("this should probably be changed so we retry periodically\n");
            vTaskDelete( NULL );
            return;
        }
        else printf("Listening on port %u\n", (unsigned int)SwitchServer::port);
        while(true)
        {
            vTaskDelay(10 / portTICK_PERIOD_MS);
            Connection *new_connection = Connection::get_next_incomming();
            if(new_connection == nullptr) continue;
            printf("new connection\n");
            xTaskCreate(SwitchServer::Connection_Handler, "Con_handler", 4000, new_connection, 10, NULL);
        }
}
