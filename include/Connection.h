#pragma once
//Encapsulates a raw lwip tcp connection. Most of it is just wrappers.
//But it also contains a read and sendall function to make
//These things eaisier.
#include <cstdint>
extern "C"
{
    #include "lwip/opt.h"
    #include "lwip/sockets.h"
    #include "lwip/sys.h"
}

class Connection;

class Connection
{
    private:
        static struct sockaddr_in bind_addr;
        static int bind_socket_fd;

        int socket_fd = -1;
        bool connected = true;
    public:
        static bool bind_to(uint16_t port);
        Connection(int _socket_fd);
        ~Connection();
        void close_connection();
        bool is_connected();
        bool recieve_all(void *data, uint16_t data_len);
        bool send_all(void *data, uint16_t data_len);
        static Connection *get_next_incomming();
        void get_client_ip(char *buf);
        uint16_t get_client_port();
};
