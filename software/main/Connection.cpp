#include "Connection.h"
#include <cstring>

struct sockaddr_in Connection::bind_addr;
int Connection::bind_socket_fd;

bool Connection::bind_to(uint16_t port)
{
    //Create the address
    Connection::bind_socket_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (Connection::bind_socket_fd == -1)
    {
      printf("cannot create socket");
      return false;
    }
    memset(&(Connection::bind_addr), 0, sizeof(Connection::bind_addr));
    Connection::bind_addr.sin_family = AF_INET;
    Connection::bind_addr.sin_port = htons(port);
    Connection::bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(Connection::bind_socket_fd,(struct sockaddr *)&(Connection::bind_addr), sizeof(Connection::bind_addr)) == -1) {
      printf("bind failed");
      close(Connection::bind_socket_fd);
      return false;
    }

    if (listen(Connection::bind_socket_fd, 10) == -1) {
      printf("listen failed");
      close(Connection::bind_socket_fd);
      return false;
    }
    return true;
}

Connection::Connection(int _socket_fd)
{
    this->socket_fd = _socket_fd;
}


Connection::~Connection()
{
    this->close_connection();
}


bool Connection::is_connected()
{
    return this->connected;
}

void Connection::close_connection()
{
    printf("closing socket\n");
    close(this->socket_fd);
    this->connected = false;
    return;
}

Connection *Connection::get_next_incomming()
{
    int new_socket = accept(Connection::bind_socket_fd, NULL, NULL);
    if (0 > new_socket)
    {
        perror("accept failed");
        close(Connection::bind_socket_fd);
        return nullptr;
    }
    return new Connection(new_socket);
}

bool Connection::recieve_all(void *data, uint16_t data_len)
{
    uint16_t recieved = 0;
    while(recieved < data_len)
    {
        uint16_t read_bytes = read(this->socket_fd, (uint8_t *)data + recieved, data_len - recieved);
        if(read_bytes == 0)
        {
            this->close_connection();
            return false;
        }
        recieved += read_bytes;
    }
    return true;
}

bool Connection::send_all(void *data, uint16_t data_len)
{
    uint16_t send_total = 0;
    while(send_total < data_len)
    {
        uint16_t send_bytes = send(this->socket_fd, (uint8_t *)data + send_total, data_len - send_total, 0);
        if(send_bytes == 0)
        {
            this->close_connection();
            return false;
        }
        send_total += send_bytes;
    }
    return true;
}

void Connection::get_client_ip(char *buf)
{
    sockaddr_in client_addr;
    socklen_t sock_len = sizeof(sockaddr_in);
    __attribute__((unused)) int get_client_addr = getpeername(this->socket_fd, (sockaddr *)&client_addr, &sock_len );
    strcpy(buf, inet_ntoa(client_addr.sin_addr));
}

uint16_t Connection::get_client_port()
{
    sockaddr_in client_addr;
    socklen_t sock_len = sizeof(sockaddr_in);
    __attribute__((unused))int get_client_addr = getpeername(this->socket_fd, (sockaddr *)&client_addr, &sock_len);
    return ntohs(client_addr.sin_port);
}
