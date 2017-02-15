#pragma once
#include <cstdint>
#include "mongoose.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"


struct OTA_status
{
    esp_ota_handle_t out_handle;
    esp_partition_t operate_partition;
};

class HttpServer
{
    public:
        HttpServer(const char *_port, bool use_ssl = false);
        ~HttpServer();
        bool start();
        bool stop();
        void ev_handler(struct mg_connection *c, int ev, void *p);
        const char *port;
        static void http_thread_wrapper(void *PvParameters);
        static void ev_handler_wrapper(struct mg_connection *c, int ev, void *p);
        void http_thread();
        static void OTA_endpoint(struct mg_connection *c, int ev, void *p);
        static void reboot(struct mg_connection *c, int ev, void *p);
        static void index(struct mg_connection *c, int ev, void *p);
        bool ota_init();
        OTA_status ota_status;
        struct mg_serve_http_opts s_http_server_opts;
    private:
        volatile bool running;
        bool use_ssl;

};
