#include "HttpServer.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_err.h"
#include "lws_server_structs.h"
#include "cJSON.h"
#include <arpa/inet.h>

__attribute__((unused)) static const char *TAG = "HTTP_SERVER";
/*We define this here as we need to access cpp functions from it.
**Other relevant LWS structs are defined in lws_server_structs.c
*/


static const struct lws_protocols __protocols[] = {
	{
		"http-only",
		lws_callback_http_dummy,
		0,	/* per_session_data_size */
		0, 0, NULL, 900
	},
    {
        "get",
        HttpServer::get_callback,
        sizeof(get_api_session_data),	/* per_session_data_size */
		0, 1, NULL, 900
    },
    {
        "post",
        HttpServer::post_callback,
        sizeof(post_api_session_data),	/* per_session_data_size */
		0, 1, NULL, 900
    },
    { \
        "ota", \
        HttpServer::ota_callback, \
        sizeof(per_session_data_ota), \
        4096, 0, NULL, 900 \
    },
    { \
        "login", \
        HttpServer::login_callback, \
        sizeof(post_api_session_data), \
        4096, 0, NULL, 900 \
    },

	{ NULL, NULL, 0, 0, 0, NULL, 0 } /* terminator */
};


HttpServer::HttpServer(const char *_port, bool _use_ssl) :
port(_port), use_ssl(_use_ssl), s_handler(SettingsHandler::get_instance()),
switch_handler(SwitchHandler::get_instance()), t_keeper(TimeKeeper::get_instance()),
cur_measurer(CurrentMeasurer::get_instance()), login_manager(LoginManager::get_instance())
{
    memset(&this->info, 0, sizeof(this->info));
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
    this->info.user = this;
    this->info.port = 80;
	this->info.fd_limit_per_thread = 30;
	this->info.max_http_header_pool = 3;
	this->info.max_http_header_data = 1024;
	this->info.pt_serv_buf_size = 4096;
	this->info.keepalive_timeout = 30;
	this->info.timeout_secs = 30;
	this->info.simultaneous_ssl_restriction = 3;
	this->info.options = LWS_SERVER_OPTION_EXPLICIT_VHOSTS;

	this->info.vhost_name = "ap";
	this->info.protocols = __protocols;
	this->info.mounts = &__mount_fileserve;
	this->info.pvo = &__get_pvo;
	this->info.headers = &__pvo_headers;
    printf("%d\n", __LINE__);
    this->context = lws_esp32_init(&this->info);
    printf("%d\n", __LINE__);
    this->running = true;
	while (!lws_service(this->context, 1000) && this->running)
    {
        if(this->do_reboot != 0 && this->do_reboot++ >= 3)
            esp_restart();
        taskYIELD();
    }

}

bool HttpServer::start()
{
    xTaskCreatePinnedToCore(HttpServer::http_thread_wrapper, "http_thread", 15000, (void **)this, 10, NULL, 0);
    return true;
}

bool HttpServer::stop()
{
    this->running = false;
    return true;
}
