#include "Message.h"

Message::Message(Message_Type type, uint8_t *_payload, uint16_t _payload_length)
{
    this->msg_type = type;
    this-> payload = new uint8_t[_payload_length];
    memcpy(this->payload, _payload, _payload_length);
    this->payload_length = _payload_length;
}


Message::Message(uint32_t _destination, uint32_t _source, Message_Type _msg_type,
    uint16_t _payload_len, uint8_t *_payload)
{
    this->destination = _destination;
    this->source = _source;
    this->msg_type = _msg_type;
    this->payload = _payload;
    this->payload_length = _payload_len;
}

Message::~Message()
{
    delete[] this->payload;
}

bool Message::send_message(uint32_t destination_id, Connection *connection)
{
    //first send the header
    uint16_t msg_len = this->payload_length + MSG_HEADER_SIZE;
    uint32_t src_id = 0; //Just enter 0 for now. Should be changed.
    bool success = true;
    uint16_t msg_len_net = htons(msg_len);
    uint32_t dest_net = htonl(destination_id);
    uint32_t src_net = htonl(src_id);
    uint16_t type_new = htons(this->msg_type);
    success &= connection->send_all(&msg_len_net, 2);
    success &= connection->send_all(&dest_net, 4);
    success &= connection->send_all(&src_net, 4);
    success &= connection->send_all(&type_new, 2);
    success &= connection->send_all(this->payload, this->payload_length);
    return success;
}

Message *Message::receive_message(Connection *connection)
{
    uint16_t msg_len;
    uint32_t dest_id;
    uint32_t src_id;
    Message_Type msg_type;
    if(!connection->recieve_all(&msg_len, 2)) return nullptr;
    if(!connection->recieve_all(&dest_id, 4)) return nullptr;
    if(!connection->recieve_all(&src_id, 4)) return nullptr;
    if(!connection->recieve_all(&msg_type, 2)) return nullptr;
    msg_len = ntohs(msg_len);
    msg_type = (Message_Type)ntohs(msg_type);
    dest_id = ntohl(dest_id);
    src_id = ntohl(src_id);
    //printf("%d %d %d %d\n", msg_len, dest_id, src_id, msg_type);
    //alloc the payload memory
    if(msg_len < MSG_HEADER_SIZE) return nullptr;
    uint16_t payload_len = msg_len - MSG_HEADER_SIZE;
    uint8_t *payload = new uint8_t[payload_len];
    if(!connection->recieve_all(payload, payload_len)) return nullptr;
    auto new_msg = new Message(dest_id, src_id, msg_type, payload_len, payload);
    return new_msg;
}
