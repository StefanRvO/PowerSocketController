#include "MessageParser.h"

MessageParser::MessageParser(Connection *_connection)
{
    this->connection = _connection;
    parser_functions[Message_Type::ping] = MessageParser::parse_ping;
}

MessageParser::~MessageParser()
{

}

void MessageParser::add_callback(uint16_t type, callback_func callback)
{
    this->callback_functions[type] = callback;
}

uint8_t MessageParser::parse_message(Message *msg)
{
    auto msg_type = msg->get_type();
    if(this->parser_functions.count(msg_type))
    {
        this->parser_functions[msg_type](this, msg);
        return PARSED;
    }
    return NOT_PARSED;


}

uint8_t MessageParser::parse_ping(MessageParser *parser, Message *msg)
{
    //Create ack message and send it
    auto response = Message(msg->get_type(), nullptr, 0);
    response.send_message(msg->get_source(), parser->connection);
    return 0;
}

std::map<uint16_t, callback_func> MessageParser::callback_functions;

std::map<uint16_t, parse_func> MessageParser::parser_functions;
