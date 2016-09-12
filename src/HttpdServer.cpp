static const char *infoView_path = "powerswitch/info.html";

#include "HttpdServer.h"
extern "C"
{
    #include "include/httpd.h"
    #include "include/cgiwifi.h"
    #include "include/espfs.h"
    #include "include/webpages-espfs.h"
    #include "include/httpdespfs.h"
    #include "include/cgiflash.h"
}
#include "InfoView.h"


bool HttpdServer::started = false;


HttpdServer::HttpdServer(uint16_t port)
{
    if(this->started == false)
    {
        espFsInit((void*)(webpages_espfs_start));

        //InfoView view1;
        //view1.set_child_path(infoView_path);

        HttpdBuiltInUrl urls[]={
            {"/wifi", cgiRedirect, "/wifi/wifi.tpl"},
            {"/wifi/", cgiRedirect, "/wifi/wifi.tpl"},
            {"/wifi/wifiscan.cgi", cgiWiFiScan, NULL},
            {"/wifi/wifi.tpl", cgiEspFsTemplate, tplWlan},
            {"*", cgiEspFsHook, NULL}, //Catch-all cgi function for the filesystem
            {NULL, NULL, NULL}
        };
        httpdInit(urls, port);
        this->started = true;
    }
}
