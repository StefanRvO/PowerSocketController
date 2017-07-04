#ifndef OTA_PROTOCOL_H
#define OTA_PROTOCOL_H
#include <libwebsockets.h>

int callback_esplws_ota(struct lws *wsi, enum lws_callback_reasons reason,
		    void *user, void *in, size_t len);
            
struct per_session_data__esplws_ota {
	struct lws_spa *spa;
	char filename[32];
	char result[LWS_PRE + 512];
	int result_len;
	int filename_length;
	esp_ota_handle_t otahandle;
	const esp_partition_t *part;
	long file_length;
	nvs_handle nvh;
};
#endif
