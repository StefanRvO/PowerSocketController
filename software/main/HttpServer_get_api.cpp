#include "HttpServer.h"
#include "cJSON.h"
#include "esp_err.h"
#include "esp_log.h"
#include <arpa/inet.h>

__attribute__((unused)) static const char *TAG = "HTTP_SERVER_GET";


void dump_handshake_info(struct lws *wsi)
{
	int n = 0, len;
	char buf[256];
	const unsigned char *c;

	do {
		c = lws_token_to_string((lws_token_indexes)n);
		if (!c) {
			n++;
			continue;
		}

		len = lws_hdr_total_length(wsi, (lws_token_indexes)n);
		if (!len || len > sizeof(buf) - 1) {
			n++;
			continue;
		}

		lws_hdr_copy(wsi, buf, sizeof buf, (lws_token_indexes)n);
		buf[sizeof(buf) - 1] = '\0';

		printf("    %s = %s\n", (char *)c, buf);
		n++;
	} while (c);
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

int HttpServer::handle_get_user_info(get_api_session_data *session_data, char *request_uri)
{
    Session session;
    uint64_t cur_time = this->t_keeper->get_uptime_milliseconds();
    if(this->login_manager->get_session_info(&session_data->session_token, &session))
        return 1;

	cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "username", session.username);
    cJSON_AddNumberToObject(root, "type", session.u_type);
    cJSON_AddNumberToObject(root, "session_created", session.created);
    cJSON_AddNumberToObject(root, "session_lastuse", session.last_used);
    cJSON_AddNumberToObject(root, "session_expire", this->login_manager->get_expire_time(&session));
    cJSON_AddNumberToObject(root, "cur_time", cur_time);


    session_data->json_str = (unsigned char *)cJSON_PrintBuffered(root,  60, 1);
    cJSON_Delete(root);
    return 0;
}

int HttpServer::handle_get_bootinfo(get_api_session_data *session_data, char *request_uri)
{
    cJSON *root = cJSON_CreateObject();

    char buf[512];
    const esp_partition_t *part = lws_esp_ota_get_boot_partition();
    struct lws_esp32_image i;
    lws_esp32_get_image_info(part, &i, buf, sizeof(buf) - 1);
    cJSON *build = cJSON_Parse(buf);

    cJSON *partition = cJSON_CreateObject();
    cJSON_AddNumberToObject(partition, "address", part->address);
    cJSON_AddNumberToObject(partition, "size", part->size);
    cJSON_AddStringToObject(partition, "label", part->label);
    cJSON_AddNumberToObject(partition, "type", part->type);
    cJSON_AddNumberToObject(partition, "subtype", part->subtype);
    cJSON_AddBoolToObject(partition, "encrypted", part->encrypted);
    cJSON *romfs = cJSON_CreateObject();
    cJSON_AddNumberToObject(romfs, "address", i.romfs);
    cJSON_AddNumberToObject(romfs, "size", i.romfs_len);

    cJSON_AddItemToObject(root, "build", build);
    cJSON_AddItemToObject(root, "romfs", romfs);
    cJSON_AddItemToObject(root, "partition", partition);
    session_data->json_str = (unsigned char *)cJSON_PrintBuffered(root,  512, 1);
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
    if(strcmp(request_uri, "/sta_info") == 0)
        return handle_get_ip_info(session_data, request_uri, TCPIP_ADAPTER_IF_STA);
    if(strcmp(request_uri, "/ap_info") == 0)
        return handle_get_ip_info(session_data, request_uri, TCPIP_ADAPTER_IF_AP);
    if(strcmp(request_uri, "/uptime") == 0)
        return handle_get_uptime(session_data, request_uri);
    if(strcmp(request_uri, "/boot_info") == 0)
        return handle_get_bootinfo(session_data, request_uri);
    if(strcmp(request_uri, "/user_info") == 0)
        return handle_get_user_info(session_data, request_uri);
    /*if(strcmp(request_uri, "/user_list") == 0)
        return handle_get_bootinfo(session_data, request_uri);*/


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
    HttpServer *server = (HttpServer *)lws_context_user(lws_get_context(wsi));
    get_api_session_data *session_data = (get_api_session_data *)user;
	switch (reason) {
    	case LWS_CALLBACK_HTTP:
        {
            switch(server->check_session_access(wsi, &session_data->session_token))
            {
                case 0:
                    break;
                case 1:
                    goto try_to_reuse;
                case 2:
                default:
                    return 1;
            }
            p = buffer + LWS_PRE;
            end = p + sizeof(buffer) - LWS_PRE;
            ESP_LOGD(TAG, "get_callback: line %d\n", __LINE__);
            ESP_LOGI(TAG, "lws_http_serve: %s\n", (const char *)in);
            json_result = ((HttpServer *)lws_context_user(lws_get_context(wsi)))->create_get_callback_reply(session_data, (char *)in);
            if( json_result != 0 ||
                session_data->json_str == nullptr)
            {
                if(json_result == 2)
                    lws_return_http_status(wsi, HTTP_STATUS_NOT_FOUND,
                    "<html><body><h1>INVALID URI!</h1></body></html>");
                    goto try_to_reuse;
                goto header_failure;
            }
            session_data->len = strlen((char *)session_data->json_str);
            session_data->sent = 0;

            if (lws_add_http_header_status(wsi, HTTP_STATUS_OK, &p, end))
                goto header_failure;
            if (lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_SERVER,
                        (unsigned char *)"libwebsockets",
                    13, &p, end))
                goto header_failure;
            if (lws_add_http_header_by_token(wsi,
                    WSI_TOKEN_HTTP_CONTENT_TYPE, (unsigned char *)"text/json",
                    9, &p, end))
                goto header_failure;

            if (lws_add_http_header_content_length(wsi,
                                   session_data->len, &p,
                                   end))
                goto header_failure;
            if (lws_finalize_http_header(wsi, &p, end))
                goto header_failure;
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
    				return 1;
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
    		return 1;

            case LWS_CALLBACK_FILTER_NETWORK_CONNECTION:
        		/* if we returned non-zero from here, we kill the connection */
        		break;

    	case LWS_CALLBACK_SSL_INFO:
    		{
    			struct lws_ssl_info *si = (struct lws_ssl_info *)in;

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
    		return 1;

	return 0;
}
