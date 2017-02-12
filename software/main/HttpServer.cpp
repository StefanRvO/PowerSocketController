#include "HttpServer.h"

HttpServer::HttpServer(const char *_port, bool _use_ssl) :
port(_port), use_ssl(_use_ssl)
{
    return;
}

HttpServer::~HttpServer()
{
    //We should make some check here to see if we have cleaned up,
    //And do so if we have not.
}


void HttpServer::http_thread_wrapper(void *PvParameters)
{
    HttpServer *server = ((HttpServer *)PvParameters);
    server->http_thread();
}

void HttpServer::http_thread()
{
    printf("Entered HTTP thread!\n");
	struct mg_mgr mgr;
    struct mg_connection *nc;
    struct mg_bind_opts bind_opts;
    mg_mgr_init(&mgr, this);

    printf("Starting web server on port %s\n", this->port);
    if(use_ssl)
    {
        memset(&bind_opts, 0, sizeof(bind_opts));
        bind_opts.ssl_cert = "/spiffs/ssl/cert.pem";
        bind_opts.ssl_key = "/spiffs/ssl/key.pem";

        // Use bind_opts to specify SSL certificate & key file
        nc = mg_bind_opt(&mgr, this->port, this->ev_handler_wrapper, bind_opts);
    }
    else
        nc = mg_bind(&mgr, this->port, this->ev_handler_wrapper);

    if (nc == NULL)
    {
      printf("Failed to create listener!\n");
      return;
    }

    printf("Webserver bound to port %s\n", this->port);

    mg_set_protocol_http_websocket(nc);
    //Create the http thread
    //struct mg_serve_http_opts s_http_server_opts;
    memset(&(this->s_http_server_opts), 0, sizeof((this->s_http_server_opts)));
    this->s_http_server_opts.document_root = "/spiffs/html/";  // Serve spiffs fs.
    this->s_http_server_opts.enable_directory_listing = "yes";
    this->s_http_server_opts.index_files = "index.html";
    this->s_http_server_opts.url_rewrites = "/=/index.html";

    printf("Mongoose HTTP server successfully started!, serving on port %s\n", this->port);
    this->running = true;

    while(this->running)
    {
        mg_mgr_poll(&mgr, 1000);
    }
    printf("Exitting HTTP SERVER task!, running was: %d\n", this->running);
    mg_mgr_free(&mgr);
    vTaskDelete( NULL );
}

// Convert a Mongoose string type to a string.
//(borrowed from https://github.com/nkolban/esp32-snippets/)
char *mgStrToStr(struct mg_str mgStr) {
	char *retStr = (char *) malloc(mgStr.len + 1);
	memcpy(retStr, mgStr.p, mgStr.len);
	retStr[mgStr.len] = 0;
	return retStr;
} // mgStrToStr


void HttpServer::ev_handler_wrapper(struct mg_connection *c, int ev, void *p) {
    HttpServer *http_server = (HttpServer *)c->mgr->user_data;
    http_server->ev_handler(c, ev, p);
}

void HttpServer::ev_handler(struct mg_connection *c, int ev, void *p)
{
    if (ev == MG_EV_HTTP_REQUEST)
    {
        struct http_message *hm = (struct http_message *) p;
        printf("The following uri was requested: %s\n", mgStrToStr(hm->uri));
        mg_serve_http(c, hm, this->s_http_server_opts);
        //mg_send_head(c, 200, hm->message.len, "Content-Type: text/plain");
        //mg_printf(c, "%.*s", hm->message.len, hm->message.p);
    }
}

bool HttpServer::start()
{
    xTaskCreatePinnedToCore(HttpServer::http_thread_wrapper, "http_thread", 20000, (void **)this, 10, NULL, 0);
    while(!this->running)
    { //yeah, race condition. This is probably fine
        vTaskDelay(10 / portTICK_PERIOD_MS);
        continue;
    }
    return true;
}

bool HttpServer::stop()
{
    this->running = false;
    return true;
}
