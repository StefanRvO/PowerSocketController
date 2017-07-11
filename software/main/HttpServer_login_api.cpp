#include "HttpServer.h"
#include "cJSON.h"
#include "esp_err.h"
#include "esp_log.h"
#include <arpa/inet.h>

__attribute__((unused)) static const char *TAG = "HTTP_SERVER_LOGIN";

static const char COOKIE_NAME[] = "PS_sid=";
static const char COOKIE_SUFFIX[] = "_sid";
static const char COOKIE_OPTIONS[] = ";Path=/";


login_error HttpServer::handle_login(post_api_session_data *session_data)
{
    if(strcmp(session_data->post_uri, "/logout") == 0)
        return this->login_manager->logout(&session_data->session_token);
    else if(strcmp(session_data->post_uri, "/login") == 0)
        return this->try_login(session_data);

    return fatal_error;
}

void HttpServer::read_session(struct lws *wsi, session_key *dest)
{
    char cookie_buff[256];
    char session_key_tmp[sizeof(session_key) + 5];
    char *p = cookie_buff;
    char *session_id_str = nullptr;

    /* fail it on no cookie */
	if (!lws_hdr_total_length(wsi, WSI_TOKEN_HTTP_COOKIE)) {
		lwsl_info("%s: no cookie\n", __func__);
		return;
	}
	if (lws_hdr_copy(wsi, cookie_buff, sizeof(cookie_buff), WSI_TOKEN_HTTP_COOKIE) < 0) {
		lwsl_info("cookie copy failed\n");
		return;
	}
    //Try to find the right cookie from the list
    while (*p) {
		if( strncmp(COOKIE_NAME, p, sizeof(COOKIE_NAME) - 1) == 0 ) {
			p += sizeof(COOKIE_NAME) - 1;
			break;
		}
		p++;
	}

    if(!*p)
    {   //No session cookie found
        lwsl_info("Cookie name not found\n");
        return;
    }
    //Find the end of the cookie

    session_id_str = p;
    while (*p) {
		if( strncmp(COOKIE_SUFFIX, p, sizeof(COOKIE_SUFFIX) - 1) == 0 ) {
			break;
		}
		p++;
	}
    if(!*p)
    {   //Suffix not found
        lwsl_info("Cookie malformated\n");
        return;
    }
    *p = '\0';
    ESP_LOGI(TAG, "%d, %s", __LINE__, session_id_str);
    int decode_result = lws_b64_decode_string(session_id_str, session_key_tmp, sizeof(session_key_tmp));
    if(decode_result < 0)
    {
        ESP_LOGI(TAG, "Cookie malformated %d, %d", __LINE__, decode_result);
        lwsl_info("Cookie malformated\n");
        return;
    }
    memcpy((char *)dest, session_key_tmp, sizeof(session_key));
}

login_error HttpServer::try_login(post_api_session_data *session_data)
{
    ESP_LOGI(TAG, "%d", __LINE__);
    login_error login_result;
    cJSON *root, *fmt, *user, *pass;
    root = cJSON_Parse(session_data->post_data);
    ESP_LOGI(TAG, "%d", __LINE__);
    if(!root) return fatal_error;
    fmt = cJSON_GetObjectItem(root, "login");
    ESP_LOGI(TAG, "%d", __LINE__);
    if(!fmt) goto try_login_failure;
    user = cJSON_GetObjectItem(fmt, "user");
    ESP_LOGI(TAG, "%d", __LINE__);
    if(!user || user->type != cJSON_String) goto try_login_failure;
    pass = cJSON_GetObjectItem(fmt, "passwd");
    ESP_LOGI(TAG, "%d", __LINE__);
    if(!pass || pass->type != cJSON_String) goto try_login_failure;
    login_result = this->login_manager->perform_login(user->valuestring, pass->valuestring, &session_data->session_token);
    ESP_LOGI(TAG, "%d", __LINE__);
    cJSON_Delete(root);
    return login_result;

    try_login_failure:
        cJSON_Delete(root);
        return fatal_error;

}
int HttpServer::check_session_access(struct lws *wsi, session_key *session_token)
{
    this->read_session(wsi, session_token); //populate token from cookie
    if(this->login_manager->is_valid(session_token) == no_error)
        return 0;
    else
    {
        if (lws_return_http_status(wsi, 401, "You need to log in!"))
            return 2;
        return 1;
    }

}

int HttpServer::login_callback(struct lws *wsi, enum lws_callback_reasons reason,
		    void *user, void *in, size_t len)
{
    unsigned char buffer[1024 + LWS_PRE];
    unsigned char *p, *end;
    unsigned char b64[SESSION_ID_MAX_ENCODED_LEN + sizeof(COOKIE_NAME) + sizeof(COOKIE_SUFFIX) + sizeof(COOKIE_OPTIONS)];
    unsigned char *b64_cur =  b64;
    int n, encoding_result;
    p = buffer + LWS_PRE;
    end = p + sizeof(buffer) - LWS_PRE;
    HttpServer *server = (HttpServer *)lws_context_user(lws_get_context(wsi));
    post_api_session_data *session_data = (post_api_session_data *)user;
    login_error login_result;

	switch (reason) {
    case LWS_CALLBACK_HTTP:
        ESP_LOGI(TAG, "LWS_CALLBACK_HTTP");
        strncpy(session_data->post_uri, (const char*)in, sizeof(session_data->post_uri));
        server->read_session(wsi, &session_data->session_token);
        break;
	case LWS_CALLBACK_HTTP_BODY:
        ESP_LOGI(TAG, "LWS_CALLBACK_HTTP_BODY");

		/* let it parse the POST data */
        if(sizeof(session_data->post_data) - 1 - session_data->total_post_length < len) //We substract 1 to make space for zero termination
            return 1;
        memcpy(session_data->post_data + session_data->total_post_length, in, len);
        session_data->total_post_length += len;
		break;

	case LWS_CALLBACK_HTTP_BODY_COMPLETION:
        ESP_LOGI(TAG, "LWS_CALLBACK_HTTP_BODY_COMPLETION");

        if(sizeof(session_data->post_data) - session_data->total_post_length < len)
            return 1;
        memcpy(session_data->post_data + session_data->total_post_length, in, len);
        session_data->total_post_length += len;

		lws_callback_on_writable(wsi);
		break;

	case LWS_CALLBACK_HTTP_WRITEABLE:
        ESP_LOGI(TAG, "LWS_CALLBACK_HTTP_WRITEABLE");

        login_result = server->handle_login(session_data);
        ESP_LOGI(TAG, "LOGIN_RESULT:%d", login_result);
        switch(login_result)
        {
            case no_error:
                ESP_LOGI(TAG, "%d", __LINE__);
                strncpy((char*)b64_cur, (char*)COOKIE_NAME, sizeof(COOKIE_NAME) - 1);
                b64_cur += sizeof(COOKIE_NAME) - 1;
                encoding_result = lws_b64_encode_string((const char*)&session_data->session_token, sizeof(session_data->session_token), (char *)b64_cur, b64 + sizeof(b64) -b64_cur);
                if(encoding_result < 0)
                    return 1;
                b64_cur += encoding_result;
                ESP_LOGI(TAG, "%d", __LINE__);
                strncpy((char*)b64_cur, COOKIE_SUFFIX, sizeof(COOKIE_SUFFIX) - 1);
                b64_cur += sizeof(COOKIE_SUFFIX) - 1;
                strncpy((char*)b64_cur, COOKIE_OPTIONS, sizeof(COOKIE_OPTIONS) - 1);
                b64_cur += sizeof(COOKIE_OPTIONS) - 1;
                *b64_cur = '\0';
                ESP_LOGI(TAG, "%d -- %s", __LINE__, b64);
                if (lws_add_http_header_status(wsi, HTTP_STATUS_OK, &p, end))
                    return 1;
                if( lws_add_http_header_by_name(wsi,
                    (unsigned char *)"set-cookie:",
                    (unsigned char *)b64, b64_cur - b64, &p,
                    (unsigned char *)end))
                    return 1;
                ESP_LOGI(TAG, "%d", __LINE__);
                if (lws_add_http_header_content_length(wsi,
                                       0, &p,
                                       end))
                    return 1;
                ESP_LOGI(TAG, "%d", __LINE__);

                if (lws_finalize_http_header(wsi, &p, end))
                    return 1;
        			*p = '\0';
                ESP_LOGI(TAG, "%d", __LINE__);

    			n = lws_write(wsi, buffer + LWS_PRE,
    				      p - (buffer + LWS_PRE),
    				      LWS_WRITE_HTTP_HEADERS);
                if (n < 0) return -1;
                p = buffer + LWS_PRE;
                ESP_LOGI(TAG, "%d", __LINE__);
    		    goto try_to_reuse;
            case user_do_not_exist:
            case invalid_password:
            case invalid_username:
            case session_invalid:
                ESP_LOGI(TAG, "%d", __LINE__);
                lws_return_http_status(wsi, HTTP_STATUS_UNAUTHORIZED, NULL);
            case fatal_error:
                ESP_LOGI(TAG, "%d", __LINE__);
                return -1;
            case logout:
                lws_return_http_status(wsi, HTTP_STATUS_OK, NULL);
                ESP_LOGI(TAG, "%d", __LINE__);
                goto try_to_reuse;
        }
        ESP_LOGI(TAG, "%d", __LINE__);
        return 1;

	case LWS_CALLBACK_HTTP_DROP_PROTOCOL:
        ESP_LOGI(TAG, "%d", __LINE__);
		break;

	default:
		break;
	}
	return 0;

try_to_reuse:
	if (lws_http_transaction_completed(wsi))
		return 1;
    ESP_LOGI(TAG, "%d", __LINE__);

	return 0;
}
