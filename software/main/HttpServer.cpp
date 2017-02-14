#include "HttpServer.h"
#include "esp_log.h"
#include "esp_system.h"


static const char *TAG = "HTTP_SERVER";


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
    this->s_http_server_opts.index_files = "index.shtml";

    mg_register_http_endpoint(nc, "/post/ota_upload", HttpServer::OTA_endpoint);
    mg_register_http_endpoint(nc, "/post/reboot", HttpServer::reboot);

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

void HttpServer::reboot(struct mg_connection *c, int ev, void *p)
{   //Endpoint for requesting a reboot.
    HttpServer *http_server = (HttpServer *)c->mgr->user_data;
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
            printf("The following uri was requested: %s\n", mgStrToStr(hm->uri));
            mg_serve_http(c, hm, this->s_http_server_opts);
            break;
        }
        default:
            break;
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
