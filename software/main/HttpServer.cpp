#include "HttpServer.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_err.h"


static const char *TAG = "HTTP_SERVER";

#define MAX_VALUE_LEN 64

bool post_parser(mg_str *key, mg_str *value, void *extra)
{
    HttpServer *server = (HttpServer *)extra;
    //Decode the form outputs
    mg_str decoded_value;
    decoded_value.p = (char *)malloc(MAX_VALUE_LEN);
    decoded_value.len = mg_url_decode(value->p, value->len, (char *)decoded_value.p, MAX_VALUE_LEN, true);
    printf("Key: %d : %.*s Value: %d : %.*s\n", key->len, key->len, key->p, decoded_value.len, decoded_value.len, decoded_value.p);

    if(strncmp(key->p, "ap_ssid", key->len) == 0)
    {
        ESP_ERROR_CHECK( server->s_handler->nvs_set("AP_SSID", decoded_value.p));
    }
    else if(strncmp(key->p, "sta_ssid", key->len) == 0)
    {
        ESP_ERROR_CHECK( server->s_handler->nvs_set("STA_SSID", decoded_value.p));
    }
    else if(strncmp(key->p, "sta_passwd", key->len) == 0)
    {
        ESP_ERROR_CHECK( server->s_handler->nvs_set("STA_PASSWORD", decoded_value.p));
    }

    free((void *)decoded_value.p);

    return true;
}

#define PARSE_KEY 0
#define PARSE_VALUE 1

bool parse_post(mg_str *str, bool ( *parser)(mg_str *, mg_str *, void *extra), void *extra)
{ //Parse the key/value pairs in the post request
    mg_str key;
    mg_str value;
    uint8_t state = PARSE_KEY;
    key.p = str->p;
    key.len = 0;
    value.p = str->p;
    value.len = 0;
    size_t i;
    for( i = 0; i <= str->len; i++)
    {
        switch(state)
        {
            case PARSE_KEY:
                if(str->p[i] == '=')
                {
                    key.len = str->p + i - key.p;
                    state = PARSE_VALUE;
                    value.p = str->p + i + 1;
                    value.len = 0;
                }
            break;
            case PARSE_VALUE:
                if(str->p[i] == '&')
                {
                    value.len = str->p + i - value.p;
                    state = PARSE_KEY;
                    if(!parser(&key, &value, extra)) return false;
                    key.p = str->p + i + 1;
                    key.len = 0;
                }
            break;
        }
    }
    //grab the final key value pair, but only if in PARSE_VALUE state
    if(state == PARSE_VALUE)
    {
        value.len = str->p + i - value.p - 1;
        if(!parser(&key, &value, extra)) return false;
    }
    return true;

}


bool HttpServer::ota_init()
{
    esp_err_t err;
    const esp_partition_t *esp_current_partition = esp_ota_get_boot_partition();
    if (esp_current_partition->type != ESP_PARTITION_TYPE_APP) {
        ESP_LOGE(TAG, "Error： esp_current_partition->type != ESP_PARTITION_TYPE_APP");
        return false;
    }

    esp_partition_t find_partition;
    memset(&this->ota_status.operate_partition, 0, sizeof(esp_partition_t));
    /*choose which OTA image should we write to*/
    switch (esp_current_partition->subtype) {
    case ESP_PARTITION_SUBTYPE_APP_FACTORY:
        find_partition.subtype = ESP_PARTITION_SUBTYPE_APP_OTA_0;
        break;
    case  ESP_PARTITION_SUBTYPE_APP_OTA_0:
        find_partition.subtype = ESP_PARTITION_SUBTYPE_APP_OTA_1;
        break;
    case ESP_PARTITION_SUBTYPE_APP_OTA_1:
        find_partition.subtype = ESP_PARTITION_SUBTYPE_APP_OTA_0;
        break;
    default:
        break;
    }
    find_partition.type = ESP_PARTITION_TYPE_APP;

    const esp_partition_t *partition = esp_partition_find_first(find_partition.type, find_partition.subtype, NULL);
    assert(partition != NULL);
    err = esp_ota_begin( partition, OTA_SIZE_UNKNOWN, &this->ota_status.out_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed err=0x%x!", err);
        return false;
    } else {
        memcpy(&this->ota_status.operate_partition, partition, sizeof(esp_partition_t));
        ESP_LOGI(TAG, "esp_ota_begin init OK");
        return true;
    }
    return false;
}


HttpServer::HttpServer(const char *_port, bool _use_ssl) :
port(_port), use_ssl(_use_ssl), s_handler(SettingsHandler::get_instance())
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
    this->s_http_server_opts.enable_directory_listing = "no";
    this->s_http_server_opts.index_files = "/index.shtml";
    //this->s_http_server_opts.per_directory_auth_file = ".htpasswd";
    this->s_http_server_opts.auth_domain = "all";
    this->s_http_server_opts.global_auth_file = "/spiffs/htpasswd";

    mg_register_http_endpoint(nc, "/post/ota_upload", HttpServer::OTA_endpoint);
    mg_register_http_endpoint(nc, "/post/reboot", HttpServer::reboot);
    mg_register_http_endpoint(nc, "/post/AP_SSID", HttpServer::SETTING);
    mg_register_http_endpoint(nc, "/post/STA_SSID", HttpServer::SETTING);

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



void HttpServer::ev_handler_wrapper(struct mg_connection *c, int ev, void *p) {
    HttpServer *http_server = (HttpServer *)c->mgr->user_data;
    http_server->ev_handler(c, ev, p);
}

void HttpServer::SETTING(struct mg_connection *c, int ev, void *p)
{
    switch(ev)
    {
        case MG_EV_HTTP_REQUEST:
        {
            struct http_message *hm = (struct http_message *) p;
            //if(strncmp(hm->method.p, "POST", hm->method.len))
            {
                printf("Got AP SSID %.*s\n", hm->method.len, hm->method.p);
                printf("Got DATA:\n %.*s\n", hm->body.len, hm->body.p);
                //Return no content code!
                parse_post(&hm->body, post_parser, c->mgr->user_data);
                mg_http_send_error(c, 204, NULL);
                break;
            }
        }

    }

}

void HttpServer::reboot(struct mg_connection *c, int ev, void *p)
{   //Endpoint for requesting a reboot.
    printf("reboot endpoint: %d\n", ev);
    switch(ev)
    {
        case MG_EV_HTTP_REQUEST:
            mg_printf(c, "Device is rebooting now!\n");
            vTaskDelay(500 / portTICK_PERIOD_MS);
            esp_restart();
            break;
    }

}
void HttpServer::index(struct mg_connection *c, int ev, void *p)
{   //Endpoint for requesting /.
    HttpServer *http_server = (HttpServer *)c->mgr->user_data;
    mg_str redirect;
    redirect.p = http_server->s_http_server_opts.index_files;
    redirect.len = strlen(redirect.p) + 1;
    mg_str empty;
    empty.p = nullptr;
    empty.len = 0;
    mg_http_send_redirect(c, 301, redirect, empty);
}


void HttpServer::OTA_endpoint(struct mg_connection *c, int ev, void *p)
{   //Endpoint for handling OTA uploads
    //We really should do some verification of the data before setting it as boot partition.
    //There is several options:
    //1. Sign the image using a private key. The device contains a public key to verify it.
    //This is un-nice for people who want to modify the firmware.
    //2. Add a simple md5 or sha sum in either the start or end of the file. We verify this as we go, and only change boot partition if verification succeds.
    HttpServer *http_server = (HttpServer *)c->mgr->user_data;
    struct mg_http_multipart_part *mp = (struct mg_http_multipart_part *)p;
    printf("Entered OTA handler\n");
    esp_err_t err;
    switch(ev)
    {
        case MG_EV_HTTP_PART_BEGIN:
        //We have recieved a new attempt at uploading OTA data.
            http_server->ota_init();
            break;
        case MG_EV_HTTP_PART_DATA:
            //OTA data incomming
            err = esp_ota_write(http_server->ota_status.out_handle, mp->data.p, mp->data.len);

            ESP_LOGI(TAG, "esp_ota_write header OK");
            break;
        case MG_EV_HTTP_PART_END:
            if (esp_ota_end(http_server->ota_status.out_handle) != ESP_OK) {
                ESP_LOGE(TAG, "esp_ota_end failed!");
                return;
            }
            err = esp_ota_set_boot_partition(&http_server->ota_status.operate_partition);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "esp_ota_set_boot_partition failed! err=0x%x", err);
            }
        break;
    }
}
void HttpServer::ev_handler(struct mg_connection *c, int ev, void *p)
{
    switch(ev)
    {
        case MG_EV_HTTP_REQUEST:
        {
            struct http_message *hm = (struct http_message *) p;
            if(strncmp(hm->uri.p, "/", hm->uri.len) == 0)
                this->index(c, ev, p);
            mg_serve_http(c, hm, this->s_http_server_opts);
            break;
        }
        case MG_EV_SSI_CALL:
            this->handle_ssi(c, p);
        default:
            break;
    }

}


void HttpServer::handle_ssi(struct mg_connection *c, void *p)
{   //Handle all SSI calls
    const char *param = (const char *) p;
    printf("entered ssid handler with param %s\n", param);
    if(strcmp(param, "get_AP_SSID") == 0)
    {
        size_t ssid_len = 0;
        ESP_ERROR_CHECK( this->s_handler->nvs_get("AP_SSID", (char *)nullptr, &ssid_len) );
        char *ap_ssid = (char *)malloc(ssid_len);
        ESP_ERROR_CHECK( this->s_handler->nvs_get("AP_SSID", ap_ssid, &ssid_len) );
        mg_printf(c, ap_ssid);
        free(ap_ssid);
    }
    else if(strcmp(param , "get_STA_SSID") == 0)
    {
        printf("%d %d \n", strlen(param + 4), strlen("get_STA_SSID"));
        size_t ssid_len = 0;
        ESP_ERROR_CHECK( this->s_handler->nvs_get("STA_SSID", (char *)nullptr, &ssid_len) );
        char *sta_ssid = (char *)malloc(ssid_len);
        ESP_ERROR_CHECK( this->s_handler->nvs_get("STA_SSID", sta_ssid, &ssid_len) );
        mg_printf(c, sta_ssid);
        free(sta_ssid);
    }
    else if(strcmp(param , "get_AP_IPV4") == 0)
    {
        tcpip_adapter_ip_info_t ip_info;
        ESP_ERROR_CHECK (tcpip_adapter_get_ip_info (TCPIP_ADAPTER_IF_AP , &ip_info));
        mg_printf(c, "IP: ");
        for(uint8_t i = 0; i < sizeof(ip4_addr_t); i++)
        {
            mg_printf(c, "%hu.", *(((uint8_t *)&ip_info.ip) + i) );
        }
        mg_printf(c, "\nnetmask: ");
        for(uint8_t i = 0; i < sizeof(ip4_addr_t); i++)
        {
            printf("%hu.", *(((uint8_t *)&ip_info.netmask) + i));
        }
        mg_printf(c,"\ngateway: ");
        for(uint8_t i = 0; i < sizeof(ip4_addr_t); i++)
        {
            mg_printf(c,"%hu.", *(((uint8_t *)&ip_info.gw) + i));
        }
        mg_printf(c,"\n");
    }
    else if(strcmp(param , "get_STA_IPV4") == 0)
    {
        tcpip_adapter_ip_info_t ip_info;
        ESP_ERROR_CHECK (tcpip_adapter_get_ip_info (TCPIP_ADAPTER_IF_STA , &ip_info));
        mg_printf(c, "IP: ");
        for(uint8_t i = 0; i < sizeof(ip4_addr_t); i++)
        {
            mg_printf(c, "%hu.", *(((uint8_t *)&ip_info.ip) + i) );
        }
        mg_printf(c, "\nnetmask: ");
        for(uint8_t i = 0; i < sizeof(ip4_addr_t); i++)
        {
            printf("%hu.", *(((uint8_t *)&ip_info.netmask) + i));
        }
        mg_printf(c,"\ngateway: ");
        for(uint8_t i = 0; i < sizeof(ip4_addr_t); i++)
        {
            mg_printf(c,"%hu.", *(((uint8_t *)&ip_info.gw) + i));
        }
        mg_printf(c,"\n");
    }
    else if(strcmp(param, "get_uptime"))
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        mg_printf(c, "Uptime: %d.%d secs\n", (int)tv.tv_sec, (int)tv.tv_usec);
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
