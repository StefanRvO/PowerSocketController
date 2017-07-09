#pragma once
#include <cstdint>
#include "esp_ota_ops.h"
#include "esp_partition.h"

#include "WifiHandler.h"
#include "SwitchHandler.h"
#include "TimeKeeper.h"
#include "CurrentMeasurer.h"
#include <libwebsockets.h>
#include "LoginManager.h"

struct OTA_status
{
    esp_ota_handle_t out_handle;
    esp_partition_t operate_partition;
};

//Struct to use for holding the session data when the /api/v1/get/ API is used
struct get_api_session_data {
	unsigned char *json_str;
    uint32_t sent;
    uint32_t len;
    session_key session_token;
};

struct post_api_session_data {
    uint32_t total_post_length;
    char post_data[1024];
    char post_uri[40];
    session_key session_token;
};


class HttpServer
{
    public:
        HttpServer(const char *_port, bool use_ssl = false);
        ~HttpServer();
        bool start();
        bool stop();
        const char *port;
        static void http_thread_wrapper(void *PvParameters);
        void http_thread();
        bool ota_init();
        OTA_status ota_status;
        static int get_callback(struct lws *wsi, enum lws_callback_reasons reason,
        		    void *user, void *in, size_t len);
        static int post_callback(struct lws *wsi, enum lws_callback_reasons reason,
        		    void *user, void *in, size_t len);
        static int login_callback(struct lws *wsi, enum lws_callback_reasons reason,
        		    void *user, void *in, size_t len);
        int create_get_callback_reply(get_api_session_data *session_data, char *request_uri);
        void print_all_sessions() {this->login_manager->print_all_sessions();}
        int check_session_access(struct lws *wsi, session_key *session_token);

    private:
        SettingsHandler *s_handler = nullptr;
        SwitchHandler *switch_handler = nullptr;
        CurrentMeasurer *cur_measurer = nullptr;
        TimeKeeper *t_keeper = nullptr;
        LoginManager *login_manager = nullptr;

        struct lws_context *context;
        struct lws_context_creation_info info;
        volatile bool running;
        bool use_ssl;
        int do_reboot = 0;
        int handle_post_data(post_api_session_data *session_data);
        int handle_get_switch_state(get_api_session_data *session_data, char *request_uri);
        int handle_get_ip_info(get_api_session_data *session_data, char *request_uri, tcpip_adapter_if_t adapter);
        int handle_get_uptime(get_api_session_data *session_data, char *request_uri);
        int handle_get_calibrations(get_api_session_data *session_data, char *request_uri);
        int handle_get_bootinfo(get_api_session_data *session_data, char *request_uri);

        int post_set_ap(post_api_session_data *session_data);
        int post_set_sta(post_api_session_data *session_data);
        int post_set_switch_state(post_api_session_data *session_data);
        login_error handle_login(post_api_session_data *session_data);
        login_error handle_try_login(post_api_session_data *session_data);
        void read_session(struct lws *wsi, session_key *dest); //NEED TO BE CALLED DURING LWS_CALLBACK_HTTP
        login_error try_login(post_api_session_data *session_data);

};
