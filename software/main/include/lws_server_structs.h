#ifndef LWS_SERVER_STRUCTS_H
#define LWS_SERVER_STRUCTS_H
#include<libwebsockets.h>
#include "lws_server_structs.h"


extern const struct lws_protocol_vhost_options __pvo_headers;
extern const struct lws_protocol_vhost_options __get_pvo;
extern const struct lws_http_mount __get_mount;

extern const struct lws_http_mount __mount_fileserve;
#endif
