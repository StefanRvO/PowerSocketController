#include "Connection.h"
#include "global_includes.h"
#define MIN_EMPTY 10

Connection::Connection(tcp_pcb *_socket)
{
    if(this->lock == NULL)
        vSemaphoreCreateBinary(this->lock);
    this->socket = _socket;
    //Create a queue for incomming packets
    this->recv_queue = xQueueCreate(IN_QUEUE_SIZE,
        sizeof(uint8_t));
    tcp_arg(this->socket, this);
    tcp_recv(this->socket, this->recieve_callback);
}

Connection::~Connection()
{
    this->close();
    vQueueDelete(recv_queue);
}

bool Connection::sendall(void *data_ptr, uint16_t data_len)
{
    printf("sending %d bytes\n", data_len);
    xSemaphoreTake(this->lock, portMAX_DELAY);
    __attribute__((unused)) err_t  err;
    uint16_t sent_bytes = 0;
    while(sent_bytes < data_len)
    {
        //Check if we have at least MIN_EMPTY bytes emty space in the buffer.
        //if not, sleep(unless the message is small enough that we can actually send it)

        uint16_t available_buffer = tcp_sndbuf(this->socket);
        while(available_buffer < 10 && data_len - sent_bytes > available_buffer)
        {
            delay_ms(1);
            available_buffer = tcp_sndbuf(this->socket);
        }
        if(data_len - sent_bytes <= available_buffer)
        {
            err = tcp_write(this->socket, (char *)data_ptr + sent_bytes,
                data_len - sent_bytes, TCP_WRITE_FLAG_COPY);
            sent_bytes = data_len;
        }
        else
        {
            err = tcp_write(this->socket, (char *)data_ptr + sent_bytes,
                available_buffer, TCP_WRITE_FLAG_COPY | TCP_WRITE_FLAG_MORE);
            sent_bytes += available_buffer;
        }
    }
    xSemaphoreGive(this->lock);
    printf("%d bytes send\n", data_len);

    return true;
}
bool Connection::is_connected()
{
    return this->connected;
}

void Connection::close()
{
    printf("closing socket\n");
    printf("socket address is %p\n", this->socket);
    this->connected = false;
    if(this->socket && this->connected)
    {
        err_t err = tcp_close(this->socket);
        //Forceclose connection if failed.
        if(err == ERR_MEM)
            tcp_abort(this->socket);
    }
}

uint16_t Connection::queue_bytes(void *data, uint16_t data_len)
{
    for(uint16_t i = 0; i < data_len; i++)
    {
        xQueueSendToBack(this->recv_queue, (uint8_t *)data + i, portMAX_DELAY);
    }

    return data_len;
}

err_t Connection::recieve_callback(void *con_object, tcp_pcb *socket, pbuf *p, err_t err)
{
    Connection *this_connection = (Connection *)con_object;
    if(p == NULL || err != 0)
    {
        delay_ms(1000);
        this_connection->close();
        return err;
    }
    do
    {
        this_connection->queue_bytes(p->payload, p->len);
        auto last_p = p;
        p = p->next;
        xSemaphoreTake(this_connection->lock, portMAX_DELAY);
        pbuf_free(last_p);
        tcp_recved(socket, last_p->len);
        xSemaphoreGive(this_connection->lock);
    } while(p != NULL);
    return ERR_OK;
}

bool Connection::recvall(void *data_ptr, uint16_t data_len)
{
    for(uint16_t i = 0; i < data_len; i++)
    {
        int ret_val = pdFALSE;
        while(this->is_connected() and ret_val == pdFALSE)
        {
            ret_val = xQueueReceive(this->recv_queue, (uint8_t *)data_ptr + i, portTICK_RATE_MS * 10);
        }
        if(ret_val == pdFALSE) return false;
    }
    return true;
}
