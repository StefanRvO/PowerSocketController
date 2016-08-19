#pragma once
#include <cstdint>
extern "C" {
#include "string.h"
}
#include "Connection.h"

#define MSG_HEADER_SIZE 12
/* This class encapsulates the message protocol used for the PowerSocket.

----------------------------------------------------------------------------
 Package len | Destination ID  |  Source ID  |  Message Type |  Payload    |
             |                 |             |               |             |
  2 bytes    | 4 bytes         |  4 bytes    |  2 Bytes      |  ? Bytes    |
             |                 |             |               |             |
----------------------------------------------------------------------------
*/
enum Message_Type : uint16_t
{
    set_switch      = 0x0000, //Set the switch state. Payload: [uint8_t switch id, uint8_t state]
    get_switch      = 0x0001, //Request switch state. Payload: [uint8_t switch id]
    switch_info     = 0x0002, //Inform about switch state. Payload [uint8_t switch id, uint8_t state]
    ping            = 0x0003, //ping msg, should answer with ping_ack
    ping_ack        = 0x0004, //ack for the ping
    set_name        = 0x0005, //Save name to device. Payload[string (0x00 terminated)]
    get_name        = 0x0006, //Request name from device.
    name_info       = 0x0007, //Message with the name of source device. Payload [string 0x00 terminated]
    get_current     = 0x0008, //Message used to request the current being drawn through the device at the moment.
    current_info    = 0x0009, //Inform about the current being drawn. Payload : [ float amps]
    set_id          = 0x0010, //Set the id of the device. Payload : [uint32_t id]
    
};

class Message
{
    private:
        Message_Type msg_type;
        uint8_t* payload = nullptr;
        uint16_t payload_length;
        uint32_t destination;
        uint32_t source;
        Message(uint32_t _destination, uint32_t _source, Message_Type _msg_type,
            uint16_t _payload_len, uint8_t *_payload);
    public:
        //Construct a message from the given payload and type.
        //The destination and source will be set when calling send_message.
        Message(Message_Type type, uint8_t *_payload, uint16_t _payload_length);
        ~Message();
        //Construct a message by recieving on the given socket. Returns a message object.
        //If the pointer returned is a nullptr, it means that the connection was closed during
        //Recieve. The caller is responisble for deleting the connection etc.
        static Message *receive_message(Connection *connection);

        //Send the message using the given socket. Fetches source from the device.
        bool send_message(uint32_t destination_id, Connection *connection);
        Message_Type get_type() { return this->msg_type; }
        uint8_t * get_payload() { return this->payload; }
        uint16_t get_payload_length() { return this->payload_length; }
        uint32_t get_dest() { return this->destination; }
        uint32_t get_source() { return this->source; }
};
