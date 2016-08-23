#pragma once
#include <cstdint>
class SwitchServer
{
    private:
        static uint16_t port;
        static void Bind_Task(void *pvParameters);
        static void Connection_Handler(void *PvParameters);

    public:
        SwitchServer() {};
        ~SwitchServer() {};
        static void start_server(uint16_t port);
};
