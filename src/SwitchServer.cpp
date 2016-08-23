#include "SwitchServer.h"
#include "Connection.h"
#include "MessageParser.h"
#include "global_includes.h"

uint16_t SwitchServer::port = 0;

void SwitchServer::start_server(uint16_t _port)
{
    SwitchServer::port = _port;
    xTaskCreate(SwitchServer::Bind_Task, (signed char *)"Bind Task", 1000, NULL, 10, NULL);
}

void SwitchServer::Connection_Handler(void *PvParameters)
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


void SwitchServer::Bind_Task(void *pvParameters)
{
        if(!Connection::bind_to(SwitchServer::port))
        {
            printf("Listen on port %u failed!\n", (unsigned int)SwitchServer::port);
        }
        else printf("Listening on port %u\n", (unsigned int)SwitchServer::port);
        while(true)
        {
            delay_ms(1);
            Connection *new_connection = Connection::get_next_incomming();
            if(new_connection == nullptr) continue;
            xTaskCreate(SwitchServer::Connection_Handler, (signed char *)"Con_handler", 1000, new_connection, 10, NULL);
        }
}
