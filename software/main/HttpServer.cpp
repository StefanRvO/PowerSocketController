#include "HttpServer.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_err.h"

static const char *TAG = "HTTP_SERVER";

#define MAX_VALUE_LEN 32

void create_digest_string(mg_str *username, mg_str *password, mg_str *realm, char *result)
{
    unsigned char hash[16];
    MD5_CTX ctx;

    MD5_Init(&ctx);
    MD5_Update(&ctx, (const unsigned char *)username->p, username->len);
    //Add a :
    MD5_Update(&ctx, (const unsigned char *)":", 1);
    //Add realm
    MD5_Update(&ctx, (const unsigned char *)realm->p, realm->len);
    //Add a :
    MD5_Update(&ctx, (const unsigned char *)":", 1);
    //Add password
    MD5_Update(&ctx, (const unsigned char *)password->p, password->len);
    MD5_Final(hash, &ctx);
    //First hash username username

    cs_to_hex(result, hash, sizeof(hash));

    return;
}

bool switch_post_parser(mg_str *key, mg_str *value, void *extra)
{
    __attribute__((unused)) HttpServer *server = (HttpServer *)extra;
    //Decode the form outputs
    mg_str decoded_value;
    decoded_value.p = (char *)malloc(MAX_VALUE_LEN);
    decoded_value.len = mg_url_decode(value->p, value->len, (char *)decoded_value.p, MAX_VALUE_LEN - 1, true);
    printf("Key: %d : %.*s Value: %d : %.*s\n", key->len, key->len, key->p, decoded_value.len, decoded_value.len, decoded_value.p);
    //Check if a switch post was recieved, if so, find out which switch it was.
    if( key->len >= 6 &&  strncmp(key->p, "switch", 6) == 0)
    {
        //The key starts with "switch", now grab the switch id.
        //We need to transform the string to a zero-terminated one, so alloc some memory for that.
        char *switch_num_str = (char *)malloc(10);
        uint8_t num_len = 9;
        if(key->len - 6 < num_len) num_len = key->len - 6;
        memcpy(switch_num_str, key->p + 6, num_len);
        //add \0 to the end
        switch_num_str[num_len] = '\0';
        unsigned int switch_num;
        if(sscanf(switch_num_str,"%u", &switch_num) != 1)
        {   //Switch number grab failed
            printf("failed switch str parse %s\n", switch_num_str);
            free((void *)decoded_value.p);
            free(switch_num_str);
            return false;
        }
        free(switch_num_str);
        //We successfully grabbed the switch number, now try to grab switch state
        //Again, zero-terminate it
        ((char *)decoded_value.p)[decoded_value.len] = '\0';
        unsigned int switch_value;
        if(sscanf(decoded_value.p,"%u", &switch_value) != 1)
        {   //Switch value grab failed
            free((void *)decoded_value.p);
            return false;
        }
        //print them
        switch_state s_state = (switch_state)switch_value;
        printf("PARSED POST, got switch num %d, switch_state %d\n", switch_num, s_state);
        //Now, set the switch state!
        server->switch_handler->set_switch_state(switch_num, s_state);
    }

    free((void *)decoded_value.p);

    return true;
}


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
    else if(strncmp(key->p, "ap_enable", key->len) == 0)
    {
        if(strncmp(value->p, "0", value->len) == 0)
        {
            printf("disable\n");
            ESP_ERROR_CHECK( server->s_handler->nvs_set("WIFI_MODE", (uint32_t)WIFI_MODE_STA));
        }
        else if(strncmp(value->p, "1", value->len) == 0)
        {
            printf("enable\n");
            ESP_ERROR_CHECK( server->s_handler->nvs_set("WIFI_MODE", (uint32_t)WIFI_MODE_APSTA));
        }
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
port(_port), use_ssl(_use_ssl), s_handler(SettingsHandler::get_instance()),
switch_handler(SwitchHandler::get_instance()), t_keeper(TimeKeeper::get_instance())
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
    mg_register_http_endpoint(nc, "/post/reset", HttpServer::reset);

    mg_register_http_endpoint(nc, "/post/AP_SSID", HttpServer::SETTING);
    mg_register_http_endpoint(nc, "/post/STA_SSID", HttpServer::SETTING);
    mg_register_http_endpoint(nc, "/post/toggle_switch", HttpServer::SWITCH_ENDPOINT);

    printf("Mongoose HTTP server successfully started!, serving on port %s\n", this->port);
    this->running = true;

    while(this->running)
    {
        if(this->do_reboot != 0 && this->do_reboot++ >= 3)
            esp_restart();
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
            {
                printf("Got AP SSID %.*s\n", hm->method.len, hm->method.p);
                printf("Got DATA:\n %.*s\n", hm->body.len, hm->body.p);
                //Return no content code!
                if(strncmp(hm->method.p, "POST", hm->method.len) == 0)
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
    HttpServer *http_server = (HttpServer *)c->mgr->user_data;
    switch(ev)
    {
        case MG_EV_HTTP_REQUEST:
            mg_http_send_error(c, 204, NULL);
            http_server->do_reboot = 1; //Start reboot countdown
            break;
    }

}

void HttpServer::SWITCH_ENDPOINT(struct mg_connection *c, int ev, void *p)
{   //Endpoint for requesting a reboot.
    printf("Switch endpoint: %d\n", ev);

    __attribute__((unused))HttpServer *http_server = (HttpServer *)c->mgr->user_data;
    struct http_message *hm = (struct http_message *) p;
    printf("Got DATA:\n %.*s\n", hm->body.len, hm->body.p);

    switch(ev)
    {
        case MG_EV_HTTP_REQUEST:

            if(strncmp(hm->method.p, "POST", hm->method.len) == 0)
            {
                parse_post(&hm->body, switch_post_parser, c->mgr->user_data);
            }
            mg_http_send_error(c, 204, NULL);
            break;
    }

}


void HttpServer::reset(struct mg_connection *c, int ev, void *p)
{   //Endpoint for requesting a reboot.
    printf("reset endpoint: %d\n", ev);
    HttpServer *http_server = (HttpServer *)c->mgr->user_data;
    switch(ev)
    {
        case MG_EV_HTTP_REQUEST:
            mg_http_send_error(c, 204, NULL);
            http_server->do_reboot = 1; //Start reboot countdown
            //Perform nvs reset
            http_server->s_handler->reset_settings();

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
            mg_http_send_error(c, 204, NULL);

        break;
    }
}

void HttpServer::handle_get_ip_info(struct mg_connection *c, struct http_message *hm, tcpip_adapter_if_t adapter)
{
    if(adapter != TCPIP_ADAPTER_IF_AP and adapter != TCPIP_ADAPTER_IF_STA)
        return;
    HttpServer *http_server = (HttpServer *)c->mgr->user_data;
    /* Send headers */
    mg_printf(c,"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: keep-alive:\r\nTransfer-Encoding: chunked\r\n\r\n");
    //Send result as json. We send ip, gateway, netmask enabled and ssid_name
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
        ESP_ERROR_CHECK( http_server->s_handler->nvs_get("AP_SSID", (char *)nullptr, &ssid_len) );
        ssid_name = (char *)malloc(ssid_len);
        ESP_ERROR_CHECK( http_server->s_handler->nvs_get("AP_SSID", ssid_name, &ssid_len) );
        //Se if the AP is enabled
        wifi_mode_t mode;
        ESP_ERROR_CHECK( http_server->s_handler->nvs_get("WIFI_MODE", (uint32_t *)(&mode)));
        printf("WIFI_MODE: %d\n", mode);
        if(mode == WIFI_MODE_APSTA)
            enabled = 1;
    }
    if(adapter == TCPIP_ADAPTER_IF_STA)
    {
        size_t ssid_len = 0;
        ESP_ERROR_CHECK( http_server->s_handler->nvs_get("STA_SSID", (char *)nullptr, &ssid_len) );
        ssid_name = (char *)malloc(ssid_len);
        ESP_ERROR_CHECK( http_server->s_handler->nvs_get("STA_SSID", ssid_name, &ssid_len) );
        //For now, station mode is always enabled
        enabled = 1;
    }

    mg_printf_http_chunk(c, "{ \"enabled\":%hhu, \"ssid\":\"%s\", \n\"ip\": \"%s\",\n\"gw\": \"%s\",\n \"nm\": \"%s\" }",
        enabled, ssid_name, ip_str, gw_str, nm_str);

    mg_send_http_chunk(c, "", 0); /* Send empty chunk, the end of response */

    free(ssid_name); //This is allocated in the above if - else blocks (adapter == something).
                     //This also means that the adapter NEEDS to be one of the types we are checking or,
                     //We will free an unallocated piece of memory (very bad!).
                     //Check at function entry if the adapter is one of the supported ones..
    free(ip_str);
    free(gw_str);
    free(nm_str);
}


void HttpServer::handle_get_switch_state(struct mg_connection *c, struct http_message *hm)
{
    HttpServer *http_server = (HttpServer *)c->mgr->user_data;
    /* Send headers */
    mg_printf(c,"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: keep-alive:\r\nTransfer-Encoding: chunked\r\n\r\n");
    //Send the current state of the switches as json.
    uint8_t switch_count = http_server->switch_handler->get_switch_count();
    mg_printf_http_chunk(c, "{ \"count\":%hhu,\n ", switch_count);
    for(uint8_t i = 0; i < switch_count; i++)
    { //Send data for each switch
        switch_state state = http_server->switch_handler->get_switch_state(i);
        printf("state: %hhu", state);
        if(i + 1 == (switch_count))
            mg_printf_http_chunk(c, "\"switch%d\": { \"id\":%hhu,\n \"state\":%hhu\n }", i, i, state);
        else
            mg_printf_http_chunk(c, "\"switch%d\": { \"id\":%hhu,\n \"state\":%hhu\n }, ", i, i, state);

    }
    mg_printf_http_chunk(c, "}");

    mg_send_http_chunk(c, "", 0); /* Send empty chunk, the end of response */

}

void HttpServer::handle_get_uptime(struct mg_connection *c, struct http_message *hm)
{
    __attribute__((unused)) HttpServer *http_server = (HttpServer *)c->mgr->user_data;
    /* Send headers */
    mg_printf(c,"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: keep-alive:\r\nTransfer-Encoding: chunked\r\n\r\n");
    //Send result as json. We send ip, gateway and netmask
    struct timeval tv;
    gettimeofday(&tv, NULL);
    mg_printf_http_chunk(c, "{ \"uptime\": \"%f\"}", http_server->t_keeper->get_uptime());
    mg_send_http_chunk(c, "", 0); /* Send empty chunk, the end of response */
}

void HttpServer::ev_handler(struct mg_connection *c, int ev, void *p)
{
    switch(ev)
    {
        case MG_EV_HTTP_REQUEST:
        {
            struct http_message *hm = (struct http_message *) p;
            printf("%.*s requested\n",  hm->uri.len,  hm->uri.p);
            if(strncmp(hm->uri.p, "/", hm->uri.len) == 0)
                this->index(c, ev, p);
            else if (mg_vcmp(&hm->uri, "/api/v1/get_ap_info") == 0)
            {
                this->handle_get_ip_info(c, hm,  TCPIP_ADAPTER_IF_AP);
            }
            else if (mg_vcmp(&hm->uri, "/api/v1/get_sta_info") == 0)
            {
                this->handle_get_ip_info(c, hm,  TCPIP_ADAPTER_IF_STA);
            }
            else if (mg_vcmp(&hm->uri, "/api/v1/get_uptime") == 0)
            {
                this->handle_get_uptime(c, hm);
            }
            else if (mg_vcmp(&hm->uri, "/api/v1/get_switch_state") == 0)
            {
                this->handle_get_switch_state(c, hm);
            }



            else
                mg_serve_http(c, hm, this->s_http_server_opts);
            break;
        }
        case MG_EV_ACCEPT:
            printf("New Connection\n");
            break;
        case MG_EV_CLOSE:
            printf("Connection closed\n");
        default:
            break;

    }

}

void mg_print_ip(struct mg_connection *c, ip4_addr_t ip)
{
    for(uint8_t i = 0; i < sizeof(ip4_addr_t); i++)
    {
        if(i < 3)
            mg_printf(c, "%hu.", *(((uint8_t *)&ip) + i) );
        else
            mg_printf(c, "%hu", *(((uint8_t *)&ip) + i) );
    }
    return;
}

bool HttpServer::start()
{
    xTaskCreatePinnedToCore(HttpServer::http_thread_wrapper, "http_thread", 15000, (void **)this, 10, NULL, 0);
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
