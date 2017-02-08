/*
 * Copyright (c) 2014-2016 Cesanta Software Limited
 * All rights reserved
 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "esp_spiffs.h"
#include "esp_vfs.h"

#include "spiffs_vfs.h"
#include "logging.h"

LOG_TAG("spiffs_vfs");



static int spiffs_open_p(void *ctx, const char *path, int flags, int mode) {
  return spiffs_vfs_open((spiffs *)ctx, path, flags, mode);
}

static int spiffs_close_p(void *ctx, int fd) {
  return spiffs_vfs_close((spiffs *)ctx, fd);
}

static ssize_t spiffs_read_p(void *ctx, int fd, void *dst, size_t size) {
  return spiffs_vfs_read((spiffs *)ctx, fd, dst, size);
}

static size_t spiffs_write_p(void *ctx, int fd, const void *data, size_t size) {
  return spiffs_vfs_write((spiffs *)ctx, fd, data, size);
}

static int spiffs_stat_p(void *ctx, const char *path, struct stat *st) {
  return spiffs_vfs_stat((spiffs *)ctx, path, st);
}

static int spiffs_fstat_p(void *ctx, int fd, struct stat *st) {
  return spiffs_vfs_fstat((spiffs *)ctx, fd, st);
}

static off_t spiffs_lseek_p(void *ctx, int fd, off_t offset, int whence) {
  return spiffs_vfs_lseek((spiffs *)ctx, fd, offset, whence);
}

static int spiffs_rename_p(void *ctx, const char *src, const char *dst) {
  return spiffs_vfs_rename((spiffs *)ctx, src, dst);
}

static int spiffs_unlink_p(void *ctx, const char *path) {
  return spiffs_vfs_unlink((spiffs *)ctx, path);
}

static DIR *spiffs_opendir_p(void *ctx, const char *name) {
  return spiffs_vfs_opendir((spiffs *)ctx, name);
}

static struct dirent *spiffs_readdir_p(void *ctx, DIR *dir) {
  return spiffs_vfs_readdir((spiffs *)ctx, dir);
}

static int spiffs_closedir_p(void *ctx, DIR *dir) {
  return spiffs_vfs_closedir((spiffs *)ctx, dir);
}


/**
 * Register the VFS at the specified mount point.
 * The callback functions are registered to handle the
 * different functions that may be requested against the
 * VFS.
 */
void spiffs_registerVFS(char *mountPoint, spiffs *fs) {
	esp_vfs_t vfs;
	esp_err_t err;

	vfs.fd_offset = 0;
	vfs.flags = ESP_VFS_FLAG_CONTEXT_PTR;
	vfs.write_p  = spiffs_write_p;
	vfs.lseek_p  = spiffs_lseek_p;
	vfs.read_p   = spiffs_read_p;
	vfs.open_p   = spiffs_open_p;
	vfs.close_p  = spiffs_close_p;
	vfs.fstat_p  = spiffs_fstat_p;
	vfs.stat_p   = spiffs_stat_p;
	vfs.link_p   = NULL;
	vfs.unlink_p = spiffs_unlink_p;
	vfs.rename_p = spiffs_rename_p;
    vfs.opendir_p = spiffs_opendir_p;
    vfs.closedir_p = spiffs_closedir_p;
    vfs.readdir_p = spiffs_readdir_p;



	err = esp_vfs_register(mountPoint, &vfs, (void *)fs);
	if (err != ESP_OK) {
		LOGE("esp_vfs_register: err=%d", err);
	}
} // spiffs_registerVFS
