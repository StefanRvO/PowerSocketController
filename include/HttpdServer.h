#pragma once
#include <cstdint>



class HttpdServer
{
    private:
        static bool started;
    public:
        HttpdServer(uint16_t port);
        ~HttpdServer() {};
};
