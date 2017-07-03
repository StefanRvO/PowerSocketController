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

const struct lws_protocol_vhost_options __get_pvo = {
	NULL,
	NULL,
	"get",
	""
};
const struct lws_http_mount __get_mount = {
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
