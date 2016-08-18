#pragma once
//Encapsulates a raw lwip tcp connection. Most of it is just wrappers.
//But it also contains a read and sendall function to make
//These things eaisier.
#include <cstdint>
extern "C"
{
#include "lwip/tcp.h"
#include "lwip/sockets.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
}

#define IN_QUEUE_SIZE 300

class Connection
{
    private:
        tcp_pcb *socket;
        xQueueHandle recv_queue;
        bool connected = true;
        xSemaphoreHandle lock = NULL;
    public:
        static err_t recieve_callback(void *con_object, tcp_pcb *socket, pbuf *p, err_t err);
        uint16_t queue_bytes(void *data, uint16_t data_len);
        Connection(tcp_pcb *_socket);
        ~Connection();
        bool sendall(void *data_ptr, uint16_t data_len); //Blocking
        bool recvall(void *data_ptr, uint16_t len); //Blocking
        void close();
        bool is_connected();
};
