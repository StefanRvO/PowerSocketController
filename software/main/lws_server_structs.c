#include "lws_server_structs.h"
/*********************************/
/*CONST STRUCT DEFINITIONS*/
/*********************************/

const struct lws_protocol_vhost_options __pvo_headers = {
	NULL,
	NULL,
	"Keep-Alive:",
	"timeout=15, max=20",
};

const struct lws_protocol_vhost_options __ota_pvo = {
	NULL,
	NULL,
	"esplws-ota",
	""
};

const struct lws_protocol_vhost_options __post_pvo = {
	&__ota_pvo,
	NULL,
	"post",
	""
};

const struct lws_protocol_vhost_options __get_pvo = {
	&__post_pvo,
	NULL,
	"get",
	""
};

const struct lws_http_mount __ota_mount = {
        .mount_next = NULL,
	    .mountpoint		= "/api/v1/ota/",
        .origin			= "ota",
        .origin_protocol	= LWSMPRO_CALLBACK,
        .mountpoint_len		= 11,
        .protocol = "ota",
};


const struct lws_http_mount __post_mount = {
        .mount_next = &__ota_mount,
	    .mountpoint		= "/api/v1/post/",
        .origin			= "post",
        .origin_protocol	= LWSMPRO_CALLBACK,
        .mountpoint_len		= 12,
        .protocol = "post",
};

const struct lws_http_mount __get_mount = {
        .mount_next     = &__post_mount,
	    .mountpoint		= "/api/v1/get/",
        .origin			= "get",
        .origin_protocol	= LWSMPRO_CALLBACK,
        .mountpoint_len		= 11,
        .protocol = "get",
};

const struct lws_http_mount __mount_fileserve = {
	.mount_next		= &__get_mount,
	.mountpoint		= "/",
    .origin			= "/html.zip",
    .def			= "index.html",
    .origin_protocol	= LWSMPRO_FILE,
    .mountpoint_len		= 1,
	.cache_max_age		= 300000,
	.cache_reusable		= 1,
    .cache_revalidate       = 1,
    .cache_intermediaries   = 1,
};
