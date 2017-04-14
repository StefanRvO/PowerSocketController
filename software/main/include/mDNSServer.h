#pragma once
#include "mdns.h"


class mDNSServer;

class mDNSServer
{
    public:
        static mDNSServer *get_instance();
    private:
        static mDNSServer *instance;
        mdns_server_t *server = nullptr;
        mDNSServer();
};
