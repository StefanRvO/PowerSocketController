/*
 * ESP32 OTA update protocol handler
 *
 * Copyright (C) 2017 Andy Green <andy@warmcat.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation:
 *  version 2.1 of the License.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA  02110-1301  USA
 *
 */
#include <string.h>
#include <esp_partition.h>
#include <esp_ota_ops.h>
#include <nvs.h>
#include "HttpServer.h"

struct per_vhost_data__esplws_ota {
	struct lws_context *context;
	struct lws_vhost *vhost;
	const struct lws_protocols *protocol;
};

static const char * const ota_param_names[] = {
	"upload",
};

enum enum_ota_param_names {
	EPN_UPLOAD,
};

const esp_partition_t *
ota_choose_part(void)
{
	const esp_partition_t *bootpart, *part = NULL;
	esp_partition_iterator_t i;

	bootpart = lws_esp_ota_get_boot_partition();
	i = esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL);
	while (i) {
		part = esp_partition_get(i);

		/* cannot update ourselves */
		if (part == bootpart)
			goto next;

		if (part->subtype < ESP_PARTITION_SUBTYPE_APP_OTA_MIN ||
		    part->subtype >= ESP_PARTITION_SUBTYPE_APP_OTA_MIN +
		    		     ESP_PARTITION_SUBTYPE_APP_OTA_MAX)
			goto next;

		break;

next:
		i = esp_partition_next(i);
	}

	if (!i) {
		lwsl_err("Can't find good OTA part\n");
		return NULL;
	}
	lwsl_notice("Directing OTA to part type %d/%d start 0x%x\n",
			part->type, part->subtype,
			(uint32_t)part->address);

	return part;
}

static int
ota_file_upload_cb(void *data, const char *name, const char *filename,
	       char *buf, int len, enum lws_spa_fileupload_states state)
{
	struct per_session_data_ota *pss =
			(struct per_session_data_ota *)data;

    printf("ota_file_upload, state: %d\n", state);
	switch (state) {
	case LWS_UFS_OPEN:
		lwsl_notice("LWS_UFS_OPEN Filename %s\n", filename);
		strncpy(pss->filename, filename, sizeof(pss->filename) - 1);
		if (strcmp(name, "ota"))
			return 1;

		pss->part = ota_choose_part();
		if (!pss->part)
			return 1;
		if (esp_ota_begin(pss->part, (long)-1, &pss->otahandle) != ESP_OK) {
			lwsl_err("OTA: Failed to begin\n");
			return 1;
		}

		pss->file_length = 0;
		break;

	case LWS_UFS_FINAL_CONTENT:
	case LWS_UFS_CONTENT:
        if(pss->part == nullptr)
        {
            lwsl_err("OTA: Partition error, maybe wrong attribute name?\n");
			return 1;
        }
		if (pss->file_length + len > pss->part->size) {
			lwsl_err("OTA: incoming file too large\n");
			return 1;
		}

		lwsl_debug("writing 0x%lx... 0x%lx\n",
			   pss->part->address + pss->file_length,
			   pss->part->address + pss->file_length + len);
		if (esp_ota_write(pss->otahandle, buf, len) != ESP_OK) {
			lwsl_err("OTA: Failed to write\n");
			return 1;
		}
		pss->file_length += len;

		if (state == LWS_UFS_CONTENT)
			break;

		lwsl_notice("LWS_UFS_FINAL_CONTENT\n");
		if (esp_ota_end(pss->otahandle) != ESP_OK) {
			lwsl_err("OTA: end failed\n");
			return 1;
		}

		if (esp_ota_set_boot_partition(pss->part) != ESP_OK) {
			lwsl_err("OTA: set boot part failed\n");
			return 1;
		}
		break;
	}

	return 0;
}

int
HttpServer::ota_callback(struct lws *wsi, enum lws_callback_reasons reason,
		    void *user, void *in, size_t len)
{
	struct per_session_data_ota *pss =
			(struct per_session_data_ota *)user;
	struct per_vhost_data__esplws_ota *vhd =
			(struct per_vhost_data__esplws_ota *)
			lws_protocol_vh_priv_get(lws_get_vhost(wsi),
					lws_get_protocol(wsi));
	unsigned char buf[LWS_PRE + 384], *start = buf + LWS_PRE - 1, *p = start,
	     *end = buf + sizeof(buf) - 1;
	int n, login_result;
    HttpServer *server = (HttpServer *)lws_context_user(lws_get_context(wsi));

	switch (reason) {

	case LWS_CALLBACK_PROTOCOL_INIT:
		vhd = (per_vhost_data__esplws_ota *)lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi),
				lws_get_protocol(wsi),
				sizeof(struct per_vhost_data__esplws_ota));
		vhd->context = lws_get_context(wsi);
		vhd->protocol = lws_get_protocol(wsi);
		vhd->vhost = lws_get_vhost(wsi);
		break;

	case LWS_CALLBACK_PROTOCOL_DESTROY:
		if (!vhd)
			break;
		break;

	/* OTA POST handling */
    case LWS_CALLBACK_HTTP:
        printf("LWS_CALLBACK_HTTP\n");
        strncpy(pss->post_uri, (const char*)in, sizeof(pss->post_uri));
        login_result = server->check_session_access(wsi, &pss->session_token);
        switch(server->check_session_access(wsi, &pss->session_token))
        {
            case 0:
                pss->allowed_to_flash = true;
                break;
            case 1:
            case 2:
            default:
                pss->allowed_to_flash = false;
                break;
        }
        break;

	case LWS_CALLBACK_HTTP_BODY:
		/* create the POST argument parser if not already existing */
        printf(" allowed %d\n", pss->allowed_to_flash);
        if(pss->allowed_to_flash == false) break;
        lwsl_notice("LWS_CALLBACK_HTTP_BODY (ota) %d %d %p\n", (int)pss->file_length, (int)len, pss->spa);
        if (!pss->spa) {
			pss->spa = lws_spa_create(wsi, ota_param_names,
					ARRAY_SIZE(ota_param_names), 4096,
					ota_file_upload_cb, pss);
			if (!pss->spa)
				return -1;

			pss->filename[0] = '\0';
			pss->file_length = 0;
		}

		/* let it parse the POST data */
        printf("%p, %p, %p, %d\n", pss->spa, pss, in, len);
		if (lws_spa_process(pss->spa, (const char*)in, len))
			return -1;
		break;

	case LWS_CALLBACK_HTTP_BODY_COMPLETION:
		lwsl_notice("LWS_CALLBACK_HTTP_BODY_COMPLETION (ota)\n");
        if(pss->allowed_to_flash == false)
        {
            if (lws_return_http_status(wsi, 401, "You need to log in!"))
                goto bail;

            goto try_to_reuse;
            break;
        }
		/* call to inform no more payload data coming */
		lws_spa_finalize(pss->spa);

		pss->result_len = snprintf(pss->result + LWS_PRE, sizeof(pss->result) - LWS_PRE - 1,
			"OTA Upgrade Complete");

		if (lws_add_http_header_status(wsi, HTTP_STATUS_OK, &p, end))
			goto bail;

		if (lws_add_http_header_by_token(wsi, WSI_TOKEN_HTTP_CONTENT_TYPE,
				(unsigned char *)"text/html", 9, &p, end))
			goto bail;
		if (lws_add_http_header_content_length(wsi, pss->result_len, &p, end))
			goto bail;
		if (lws_finalize_http_header(wsi, &p, end))
			goto bail;

		n = lws_write(wsi, start, p - start, LWS_WRITE_HTTP_HEADERS);
		if (n < 0)
			goto bail;

		lws_callback_on_writable(wsi);
		break;

	case LWS_CALLBACK_HTTP_WRITEABLE:
		lwsl_debug("LWS_CALLBACK_HTTP_WRITEABLE: sending %d\n",
			   pss->result_len);
		n = lws_write(wsi, (unsigned char *)pss->result + LWS_PRE,
			      pss->result_len, LWS_WRITE_HTTP);
		if (n < 0)
			return 1;

		if (lws_http_transaction_completed(wsi))
			return 1;

		break;

	case LWS_CALLBACK_HTTP_DROP_PROTOCOL:
		/* called when our wsi user_space is going to be destroyed */
		if (pss->spa) {
			lws_spa_destroy(pss->spa);
			pss->spa = NULL;
		}
		break;

	default:
		break;
	}

	return 0;
try_to_reuse:
	if (lws_http_transaction_completed(wsi))
		return -1;
    return 0;

bail:
	return 1;
}