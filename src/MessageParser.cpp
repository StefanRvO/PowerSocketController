#include "MessageParser.h"
#include "SettingsReader.h"
extern "C"
{
   #include "espressif/esp_common.h"
}

MessageParser::MessageParser(Connection *_connection)
{
    this->connection = _connection;
    parser_functions[Message_Type::ping] = MessageParser::parse_ping;
    parser_functions[Message_Type::set_name] = MessageParser::parse_set_name;
    parser_functions[Message_Type::set_wifi_ssid] = MessageParser::parse_set_wifi_ssid;
    parser_functions[Message_Type::set_wifi_password] = MessageParser::parse_set_wifi_password;
    parser_functions[Message_Type::reboot] = MessageParser::parse_reboot;
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
    //printf("Parsing message with type : %02X\n", msg->get_type() );
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

    auto response = Message(Message_Type::ping_ack, nullptr, 0);
    response.send_message(msg->get_source(), parser->connection);
    return 0;
}

uint8_t MessageParser::parse_set_name(MessageParser *parser, Message *msg)
{
    SettingsReader::set_name((char *)msg->get_payload(), msg->get_payload_length());
}
uint8_t MessageParser::parse_set_wifi_ssid(MessageParser *parser, Message *msg)
{
    SettingsReader::set_wifi_ssid((char *)msg->get_payload(), msg->get_payload_length());
}
uint8_t MessageParser::parse_set_wifi_password(MessageParser *parser, Message *msg)
{
    SettingsReader::set_wifi_password((char *)msg->get_payload(), msg->get_payload_length());
}
uint8_t MessageParser::parse_reboot(MessageParser *parser, Message *msg)
{
    printf("rebooting\n");
    sdk_system_restart();
}



std::map<uint16_t, callback_func> MessageParser::callback_functions;

std::map<uint16_t, parse_func> MessageParser::parser_functions;
