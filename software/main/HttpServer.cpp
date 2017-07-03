#include "HttpServer.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_err.h"
#include "lws_server_structs.h"
#include "cJSON.h"
static const char *TAG = "HTTP_SERVER";
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
	{ NULL, NULL, 0, 0, 0, NULL, 0 } /* terminator */
};


HttpServer::HttpServer(const char *_port, bool _use_ssl) :
port(_port), use_ssl(_use_ssl), s_handler(SettingsHandler::get_instance()),
switch_handler(SwitchHandler::get_instance()), t_keeper(TimeKeeper::get_instance()),
cur_measurer(CurrentMeasurer::get_instance())
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
	while (!lws_service(this->context, 10) && this->running)
                taskYIELD();

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

int HttpServer::handle_get_switch_state(get_api_session_data *session_data, char *request_uri)
{
    //Send the current state of the switches as json.
    uint8_t switch_count = this->switch_handler->get_switch_count();
    cJSON *root,*fmt;
	root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "count", switch_count);
    for(uint8_t i = 0; i < switch_count; i++)
    { //Send data for each switch
        char switch_name[10];
        snprintf(switch_name, 10, "switch%d", i);
        switch_state state = this->switch_handler->get_switch_state(i);
        cJSON_AddItemToObject(root, switch_name, fmt=cJSON_CreateObject());
        cJSON_AddNumberToObject(fmt, "id", i);
        cJSON_AddNumberToObject(fmt, "state", state);
    }
    session_data->json_str = (unsigned char *)cJSON_PrintBuffered(root,  switch_count * 20 + 15, 1);
    cJSON_Delete(root);
    return 0;
}

int HttpServer::handle_get_uptime(get_api_session_data *session_data, char *request_uri)
{
	cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "uptime", this->t_keeper->get_uptime());
    session_data->json_str = (unsigned char *)cJSON_PrintBuffered(root,  20, 1);
    cJSON_Delete(root);
    return 0;
}


int HttpServer::handle_get_calibrations(get_api_session_data *session_data, char *request_uri)
{
    cJSON *root,*fmt;
	root = cJSON_CreateObject();
    uint8_t count = this->cur_measurer->get_current_count();
    cJSON_AddNumberToObject(root, "count", count);

    for(uint8_t i = 0; i < count; i++)
    {   //Send data for each calibration
        char calib_name[10];
        CurrentCalibration cur_calib;
        this->cur_measurer->load_current_calibration(i, cur_calib);
        snprintf(calib_name, 10, "ccalib%d", i);
        cJSON_AddItemToObject(root, calib_name, fmt=cJSON_CreateObject());
        cJSON_AddNumberToObject(fmt, "id", i);
        cJSON_AddNumberToObject(fmt, "conversion",  cur_calib.conversion);
        cJSON_AddNumberToObject(fmt, "bias_on",     cur_calib.bias_on.last.bias);
        cJSON_AddNumberToObject(fmt, "bias_off",    cur_calib.bias_off.last.bias);
        cJSON_AddNumberToObject(fmt, "stddev_on",   cur_calib.bias_on.last.stddev);
        cJSON_AddNumberToObject(fmt, "stddev_off",  cur_calib.bias_off.last.stddev);
        cJSON_AddNumberToObject(fmt, "calibrated",  cur_calib.bias_on.last.completed);
    }
    session_data->json_str = (unsigned char *)cJSON_PrintBuffered(root,  count * 100 + 10, 1);
    cJSON_Delete(root);
    return 0;
}

int HttpServer::handle_get_ip_info(get_api_session_data *session_data, char *request_uri, tcpip_adapter_if_t adapter)
{
    if(adapter != TCPIP_ADAPTER_IF_AP and adapter != TCPIP_ADAPTER_IF_STA)
        return 1;
    tcpip_adapter_ip_info_t ip_info;
    ESP_ERROR_CHECK (tcpip_adapter_get_ip_info (adapter , &ip_info));
    char *ip_str, *gw_str, *nm_str, *ssid_name = nullptr;
    uint8_t enabled = 0;
    ip_str = (char *)malloc(16);
    gw_str = (char *)malloc(16);
    nm_str = (char *)malloc(16);
    strcpy(ip_str,inet_ntoa(ip_info.ip));
    strcpy(gw_str,inet_ntoa(ip_info.gw));
    strcpy(nm_str,inet_ntoa(ip_info.netmask));

    //Retrieve ssid settings
    if(adapter == TCPIP_ADAPTER_IF_AP)
    {
        size_t ssid_len = 0;
        ESP_ERROR_CHECK( this->s_handler->nvs_get("AP_SSID", (char *)nullptr, &ssid_len) );
        ssid_name = (char *)malloc(ssid_len);
        ESP_ERROR_CHECK( this->s_handler->nvs_get("AP_SSID", ssid_name, &ssid_len) );
        //Se if the AP is enabled
        wifi_mode_t mode;
        ESP_ERROR_CHECK( this->s_handler->nvs_get("WIFI_MODE", (uint32_t *)(&mode)));
        if(mode == WIFI_MODE_APSTA)
            enabled = 1;
    }
    if(adapter == TCPIP_ADAPTER_IF_STA)
    {
        size_t ssid_len = 0;
        ESP_ERROR_CHECK( this->s_handler->nvs_get("STA_SSID", (char *)nullptr, &ssid_len) );
        ssid_name = (char *)malloc(ssid_len);
        ESP_ERROR_CHECK( this->s_handler->nvs_get("STA_SSID", ssid_name, &ssid_len) );
        //For now, station mode is always enabled
        enabled = 1;
    }
    cJSON *root;
	root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "enabled", enabled);
    cJSON_AddStringToObject(root, "ssid", ssid_name);
    cJSON_AddStringToObject(root, "ip", ip_str);
    cJSON_AddStringToObject(root, "gw", gw_str);
    cJSON_AddStringToObject(root, "nm", nm_str);
    free(ssid_name); //This is allocated in the above if - else blocks (adapter == something).
                     //This also means that the adapter NEEDS to be one of the types we are checking or,
                     //We will free an unallocated piece of memory (very bad!).
                     //Check at function entry if the adapter is one of the supported ones..
    free(ip_str);
    free(gw_str);
    free(nm_str);
    session_data->json_str = (unsigned char *)cJSON_PrintBuffered(root,  100, 1);
    cJSON_Delete(root);
    return 0;
}


int HttpServer::create_get_callback_reply(get_api_session_data *session_data, char *request_uri)
{
    if(strcmp(request_uri, "/switch_state") == 0)
        return handle_get_switch_state(session_data, request_uri);
    if(strcmp(request_uri, "/current_calibrations") == 0)
        return handle_get_calibrations(session_data, request_uri);
    if(strcmp(request_uri, "/sta_info") == 0)
        return handle_get_ip_info(session_data, request_uri, TCPIP_ADAPTER_IF_STA);
    if(strcmp(request_uri, "/ap_info") == 0)
        return handle_get_ip_info(session_data, request_uri, TCPIP_ADAPTER_IF_AP);
    if(strcmp(request_uri, "/uptime") == 0)
        return handle_get_uptime(session_data, request_uri);
    return 2; //This results in a 404 being sent
}



//Inspired by the test-server-http.c example. Check it out for further details of other features of lws.
int
HttpServer::get_callback(struct lws *wsi, enum lws_callback_reasons reason,
		    void *user, void *in, size_t len)
{
    int n,m = 0;
    unsigned char buffer[1024 + LWS_PRE];
    unsigned char *p, *end;
    int json_result;
    get_api_session_data *session_data = (get_api_session_data *)user;
	switch (reason) {
    	case LWS_CALLBACK_HTTP:
        {
            p = buffer + LWS_PRE;
            end = p + sizeof(buffer) - LWS_PRE;
            ESP_LOGD(TAG, "get_callback: line %d\n", __LINE__);
            ESP_LOGI(TAG, "lws_http_serve: %s\n", (const char *)in);
            json_result = ((HttpServer *)lws_context_user(lws_get_context(wsi)))->create_get_callback_reply(session_data, (char *)in);
            if( json_result != 0 ||
                session_data->json_str == nullptr)
            {
                if(json_result == 2)
                    lws_return_http_status(wsi, HTTP_STATUS_NOT_FOUND, NULL);
                    goto try_to_reuse;
                goto header_failure;
            }
            session_data->len = strlen((char *)session_data->json_str);
            session_data->sent = 0;

            ESP_LOGD(TAG, "get_callback: line %d\n", __LINE__);
            if (lws_add_http_header_status(wsi, HTTP_STATUS_OK, &p, end))
                goto header_failure;
            ESP_LOGD(TAG, "get_callback: line %d\n", __LINE__);
            if (lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_SERVER,
                        (unsigned char *)"libwebsockets",
                    13, &p, end))
                goto header_failure;
            ESP_LOGD(TAG, "get_callback: line %d\n", __LINE__);
            if (lws_add_http_header_by_token(wsi,
                    WSI_TOKEN_HTTP_CONTENT_TYPE,
                        (unsigned char *)"text/json",
                    9, &p, end))
                goto header_failure;
            ESP_LOGD(TAG, "get_callback: line %d\n", __LINE__);
            if (lws_add_http_header_content_length(wsi,
                                   session_data->len, &p,
                                   end))
                goto header_failure;
            ESP_LOGD(TAG, "get_callback: line %d\n", __LINE__);
            if (lws_finalize_http_header(wsi, &p, end))
                goto header_failure;
            ESP_LOGD(TAG, "get_callback: line %d\n", __LINE__);
                /*
    			 * send the http headers...
    			 * this won't block since it's the first payload sent
    			 * on the connection since it was established
    			 * (too small for partial)
    			 *
    			 * Notice they are sent using LWS_WRITE_HTTP_HEADERS
    			 * which also means you can't send body too in one step,
    			 * this is mandated by changes in HTTP2
    			 */

    			*p = '\0';
    			lwsl_info("%s\n", buffer + LWS_PRE);

    			n = lws_write(wsi, buffer + LWS_PRE,
    				      p - (buffer + LWS_PRE),
    				      LWS_WRITE_HTTP_HEADERS);
    			if (n < 0) {
                    if(session_data->json_str) free(session_data->json_str);
    				return -1;
    			}
                /*Transfer the malloced json string to the buffered one.
                 *This means that we can free the memory op fairly quickly, and not worry anymore.*/

    			/*
    			 * book us a LWS_CALLBACK_HTTP_WRITEABLE callback
    			 */
    			lws_callback_on_writable(wsi);

    		break;
        }

    	case LWS_CALLBACK_HTTP_WRITEABLE:
            ESP_LOGI(TAG, "LWS_CALLBACK_HTTP_WRITEABLE\n");
            /*
    		 * we can send more of whatever it is we were sending
    		 */
    		do {
    			/* we'd like the send this much */
    			n = session_data->len - session_data->sent;
    			/* but if the peer told us he wants less, we can adapt */
    			m = lws_get_peer_write_allowance(wsi);

    			/* -1 means not using a protocol that has this info */
    			if (m == 0)
    				/* right now, peer can't handle anything */
    				goto later;

    			if (m != -1 && m < n)
    				/* he couldn't handle that much */
    				n = m;
    			/* sent it all, close conn */
    			if (n == 0)
    				goto penultimate;
    			/*
    			 * To support HTTP2, must take care about preamble space
    			 *
    			 * identification of when we send the last payload frame
    			 * is handled by the library itself if you sent a
    			 * content-length header
    			 */
    			m = lws_write(wsi, session_data->json_str + session_data->sent, n, LWS_WRITE_HTTP);
    			if (m < 0) {
    				lwsl_err("write failed\n");
    				/* write failed, close conn */
    				goto bail;
    			}
    			if (m) /* while still active, extend timeout */
    				lws_set_timeout(wsi, PENDING_TIMEOUT_HTTP_CONTENT, 5);
    			session_data->sent += m;

    		} while (!lws_send_pipe_choked(wsi) && (session_data->sent < 1024 * 1024));
    later:
    		lws_callback_on_writable(wsi);
    		break;
    penultimate:
            if(session_data->json_str) free(session_data->json_str);
    		goto try_to_reuse;
    bail:
            ESP_LOGI(TAG, "LWS WRITE FAILED, BAILING\n")

            if(session_data->json_str) free(session_data->json_str);
    		return -1;

            case LWS_CALLBACK_FILTER_NETWORK_CONNECTION:
        		/* if we returned non-zero from here, we kill the connection */
        		break;

    	case LWS_CALLBACK_SSL_INFO:
    		{
    			struct lws_ssl_info *si = in;

    			ESP_LOGI(TAG, "LWS_CALLBACK_SSL_INFO: where: 0x%x, ret: 0x%x\n",
    					si->where, si->ret);
    		}
    		break;

    	default:
    		break;
    }
    return 0;

    header_failure:
            ESP_LOGI(TAG, "SOMETHING WIERD HAPPENED WHILE SENDING THE HEADERS OR GENERATING THE JSON, BAILING\n");
            if(session_data->json_str) free(session_data->json_str);
            return 1;
    try_to_reuse:
    	if (lws_http_transaction_completed(wsi))
    		return -1;

	return 0;
}
