#pragma once
#include <cstdint>
#include "mongoose.h"

class HttpServer
{
    public:
        HttpServer(const char *_port);
        ~HttpServer();
        bool start();
        bool stop();
        void ev_handler(struct mg_connection *c, int ev, void *p);

    private:
        const char *port;
        void http_thread();
        static void http_thread_wrapper(void *PvParameters);
        struct mg_serve_http_opts s_http_server_opts;
        static void ev_handler_wrapper(struct mg_connection *c, int ev, void *p);
        volatile bool running;


};
