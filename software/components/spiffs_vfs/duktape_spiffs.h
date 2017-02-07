/*
 * duktape_spiffs.h
 *
 *  Created on: Dec 3, 2016
 *      Author: kolban
 */

#if !defined(MAIN_DUKTAPE_SPIFFS_H_)
#define MAIN_DUKTAPE_SPIFFS_H_
#include <spiffs.h>
void esp32_duktape_spiffs_mount();
void esp32_duktape_dump_spiffs();
void esp32_duktape_spiffs_write(char *fileName, uint8_t *data, int length);
void spiffs_registerVFS(char *mountPoint, spiffs *fs);

#endif /* MAIN_DUKTAPE_SPIFFS_H_ */
