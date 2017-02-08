/*
 * Copyright (c) 2014-2016 Cesanta Software Limited
 * All rights reserved
 */

#ifndef CS_FW_PLATFORMS_ESP32_SRC_ESP32_FS_H_
#define CS_FW_PLATFORMS_ESP32_SRC_ESP32_FS_H_

#include "spiffs.h"

void spiffs_registerVFS(char *mountPoint, spiffs *fs);


#endif /* CS_FW_PLATFORMS_ESP32_SRC_ESP32_FS_H_ */
