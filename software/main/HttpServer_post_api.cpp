#include "HttpServer.h"
#include "cJSON.h"
#include "esp_err.h"
#include "esp_log.h"
#include <arpa/inet.h>

__attribute__((unused)) static const char *TAG = "HTTP_SERVER_POST";


int HttpServer::post_set_sta(post_api_session_data *session_data)
{
    cJSON *root, *fmt, *item;
    root = cJSON_Parse(session_data->post_data);
    if(!root) return -1;
    fmt = cJSON_GetObjectItem(root, "sta");
    if(!fmt) goto post_set_sta_failure;
    item = cJSON_GetObjectItem(fmt, "ssid");
    if(!item || item->type != cJSON_String) goto post_set_sta_failure;
    ESP_ERROR_CHECK( this->s_handler->nvs_set("STA_SSID", item->valuestring));
    item = cJSON_GetObjectItem(fmt, "passwd");
    if(!item || item->type != cJSON_String ) goto post_set_sta_failure;
    ESP_ERROR_CHECK( this->s_handler->nvs_set("STA_PASSWORD", item->valuestring));
    cJSON_Delete(root);

    post_set_sta_failure:
        cJSON_Delete(root);
        return -1;

    return 0;
}

int HttpServer::post_set_ap(post_api_session_data *session_data)
{
    cJSON *root, *fmt, *item;
    uint32_t tmp;
    root = cJSON_Parse(session_data->post_data);
    if(!root) return -1;
    fmt = cJSON_GetObjectItem(root, "ap");
    if(!fmt) goto post_set_ap_failure;
    item = cJSON_GetObjectItem(fmt, "ssid");
    if(!item || item->type != cJSON_String) goto post_set_ap_failure;
    ESP_ERROR_CHECK( this->s_handler->nvs_set("AP_SSID", item->valuestring));
    item = cJSON_GetObjectItem(fmt, "enable");
    if(!item || item->type != cJSON_String) goto post_set_ap_failure;
    if(strcmp(item->valuestring, "0") == 0)
    {
        ESP_ERROR_CHECK( this->s_handler->nvs_set("WIFI_MODE", (uint32_t)WIFI_MODE_STA));
    }
    else if(strcmp(item->valuestring, "1") == 0)
    {
        ESP_ERROR_CHECK( this->s_handler->nvs_set("WIFI_MODE", (uint32_t)WIFI_MODE_APSTA));
    }
    else goto post_set_ap_failure;

    item = cJSON_GetObjectItem(fmt, "ip");
    if(!item || item->type != cJSON_String) goto post_set_ap_failure;
    if(!inet_aton(item->valuestring, &tmp)) goto post_set_ap_failure;
    ESP_ERROR_CHECK( this->s_handler->nvs_set("AP_IP", tmp));
    item = cJSON_GetObjectItem(fmt, "gw");
    if(!item || item->type != cJSON_String) goto post_set_ap_failure;
    if(!inet_aton(item->valuestring, &tmp)) goto post_set_ap_failure;
    ESP_ERROR_CHECK( this->s_handler->nvs_set("AP_GATEWAY", tmp));
    item = cJSON_GetObjectItem(fmt, "netmask");
    if(!item || item->type != cJSON_String) goto post_set_ap_failure;
    if(!inet_aton(item->valuestring, &tmp)) goto post_set_ap_failure;
    ESP_ERROR_CHECK( this->s_handler->nvs_set("AP_NETMASK", tmp));
    cJSON_Delete(root);

    return 0;
    post_set_ap_failure:
        cJSON_Delete(root);
        return -1;
}

int HttpServer::post_set_switch_state(post_api_session_data *session_data)
{
    cJSON *root, *item;
    uint32_t switch_num;
    root = cJSON_Parse(session_data->post_data);
    if(!root) return -1;
    for (int i = 0; i < cJSON_GetArraySize(root); i++)
    {
        item = cJSON_GetArrayItem(root, i);
        if(!(item->type == cJSON_Number)) continue;
        if(sscanf(item->string,"switch%u", &switch_num) != 1)
        {   //Switch number grab failed
            printf("failed switch str parse %s\n", item->string);
            continue;
        }
        switch_state s_state = (switch_state)item->valueint;
        //Now, set the switch state!
        this->switch_handler->set_switch_state(switch_num, s_state);
    }
    cJSON_Delete(root);

    return 0;
}


int HttpServer::handle_post_data(post_api_session_data *session_data)
{
    session_data->post_data[session_data->total_post_length] = '\0'; //Make sure to zero terminate the string
    printf("URI: %.*s\n", sizeof(session_data->post_uri), session_data->post_uri);
    if(strcmp(session_data->post_uri, "/STA_SSID") == 0)
        return this->post_set_sta(session_data);
    else if(strcmp(session_data->post_uri, "/AP_SSID") == 0)
        return this->post_set_ap(session_data);
    else if(strcmp(session_data->post_uri, "/recalib_current") == 0)
        this->cur_measurer->recalib_current_sensors();
    else if(strcmp(session_data->post_uri, "/reboot") == 0)
        this->do_reboot = 1;
    else if(strcmp(session_data->post_uri, "/reset") == 0)
        this->s_handler->reset_settings();
    else if(strcmp(session_data->post_uri, "/toggle_switch") == 0)
        return post_set_switch_state(session_data);

    return 0;
}

int
HttpServer::post_callback(struct lws *wsi, enum lws_callback_reasons reason,
		    void *user, void *in, size_t len)
{
    post_api_session_data *session_data = (post_api_session_data *)user;
    int post_result;
    printf("post callback reason: %d\n", reason);

	switch (reason) {
    case LWS_CALLBACK_HTTP:
        strncpy(session_data->post_uri, (const char*)in, sizeof(session_data->post_uri));
        printf("LWS_CALLBACK_HTTP\n");
        break;
	case LWS_CALLBACK_HTTP_BODY:
        printf("LWS_CALLBACK_HTTP_BODY\n");

		/* let it parse the POST data */
        if(sizeof(session_data->post_data) - 1 - session_data->total_post_length < len) //We substract 1 to make space for zero termination
            return -1;
        memcpy(session_data->post_data + session_data->total_post_length, in, len);
        session_data->total_post_length += len;
		break;

	case LWS_CALLBACK_HTTP_BODY_COMPLETION:
		printf("LWS_CALLBACK_HTTP_BODY_COMPLETION\n");
        if(sizeof(session_data->post_data) - session_data->total_post_length < len)
            return -1;
        memcpy(session_data->post_data + session_data->total_post_length, in, len);
        session_data->total_post_length += len;

		lws_callback_on_writable(wsi);
		break;

	case LWS_CALLBACK_HTTP_WRITEABLE:
		printf("LWS_CALLBACK_HTTP_WRITEABLE\n");
        post_result = ((HttpServer *)lws_context_user(lws_get_context(wsi)))->handle_post_data(session_data);
        if(post_result == 0)
        {
            lws_return_http_status(wsi, HTTP_STATUS_OK, NULL);
		    goto try_to_reuse;
        }
        else
            return -1;

	case LWS_CALLBACK_HTTP_DROP_PROTOCOL:
		break;

	default:
		break;
	}

	return 0;
try_to_reuse:
	if (lws_http_transaction_completed(wsi))
		return -1;

	return 0;
}
