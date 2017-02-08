/*
 * Copyright (c) 2014-2016 Cesanta Software Limited
 * All rights reserved
 */

#ifndef CS_COMMON_SPIFFS_SPIFFS_VFS_H_
#define CS_COMMON_SPIFFS_SPIFFS_VFS_H_



#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "esp_vfs.h"

#include <spiffs.h>


int spiffs_vfs_open(spiffs *fs, const char *path, int flags, int mode);
int spiffs_vfs_close(spiffs *fs, int fd);
ssize_t spiffs_vfs_read(spiffs *fs, int fd, void *dst, size_t size);
size_t spiffs_vfs_write(spiffs *fs, int fd, const void *data, size_t size);
int spiffs_vfs_stat(spiffs *fs, const char *path, struct stat *st);
int spiffs_vfs_fstat(spiffs *fs, int fd, struct stat *st);
off_t spiffs_vfs_lseek(spiffs *fs, int fd, off_t offset, int whence);
int spiffs_vfs_rename(spiffs *fs, const char *src, const char *dst);
int spiffs_vfs_unlink(spiffs *fs, const char *path);

#include <dirent.h>
DIR *spiffs_vfs_opendir(spiffs *fs, const char *name);
struct dirent *spiffs_vfs_readdir(spiffs *fs, DIR *dir);
int spiffs_vfs_closedir(spiffs *fs, DIR *dir);

int set_spiffs_errno(spiffs *fs, const char *op, int res);



#endif /* CS_COMMON_SPIFFS_SPIFFS_VFS_H_ */
