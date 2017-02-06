#pragma once
#include <cstdint>
extern "C"
{
    #include <mongoose.h>
}

class HttpServer
{
    public:
        HttpServer(const char *_port);
        ~HttpServer();
        bool start();
        bool stop();

    private:
        const char *port;
        void http_thread();
        static void http_thread_wrapper(void *PvParameters);

        static void ev_handler(struct mg_connection *c, int ev, void *p);
        volatile bool running;


};
