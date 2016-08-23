//This class parses a message and calls the given callback function with the result from the parsing.
#pragma once
#include "Message.h"
#include "Connection.h"
#include <map>
#define PARSED 0
#define NOT_PARSED 1


class MessageParser;

typedef uint8_t (*parse_func)(MessageParser *parser, Message *msg);
typedef uint8_t (*callback_func)(void);


class MessageParser
{
    private:
        static std::map<uint16_t, callback_func> callback_functions;
        static std::map<uint16_t, parse_func> parser_functions;
        Connection *connection = nullptr;
    public:
        MessageParser(Connection *_connection);
        ~MessageParser();
        void add_callback(uint16_t type, callback_func callback);

        callback_func get_callback(Message_Type msg_type)
        {
            return this->callback_functions[msg_type];
        }

        void clear_callbacks()
        {
            this->callback_functions.clear();
        }
        uint8_t parse_message(Message *msg);
        static uint8_t parse_ping(MessageParser *parser, Message *msg);
        static uint8_t parse_set_name(MessageParser *parser, Message *msg);
        static uint8_t parse_set_wifi_ssid(MessageParser *parser, Message *msg);
        static uint8_t parse_set_wifi_password(MessageParser *parser, Message *msg);
        static uint8_t parse_reboot(MessageParser *parser, Message *msg);


};
