/*
 * Copyright (c) 2014-2016 Cesanta Software Limited
 * All rights reserved
 */

/* LIBC interface to SPIFFS. */

#include <spiffs_vfs.h>

#include <errno.h>
#include <stdio.h>
#include <fcntl.h>


//Disable this function, it causes isuess. (like stat not working correctly)
static const char *drop_dir(const char *fname) {
  /* Drop "./", if any */
  /*if (fname[0] == '.' && fname[1] == '/') {
    fname += 2;
  }*/
  /*
   * Drop / if it is the only one in the path.
   * This allows use of /pretend/directories but serves /file.txt as normal.
   */
  /*if (fname[0] == '/' && strchr(fname + 1, '/') == NULL) {
    fname++;
    }*/

  return fname;
}

static int set_errno(int e) {
  errno = e;
  return (e == 0 ? 0 : -1);
}

static int spiffs_err_to_errno(int r) {
  switch (r) {
    case SPIFFS_OK:
      return 0;
    case SPIFFS_ERR_FULL:
      return ENOSPC;
    case SPIFFS_ERR_NOT_FOUND:
      return ENOENT;
    case SPIFFS_ERR_NOT_WRITABLE:
    case SPIFFS_ERR_NOT_READABLE:
      return EACCES;
  }
  return ENXIO;
}

int set_spiffs_errno(spiffs *fs, const char *op, int res) {
  int e = SPIFFS_errno(fs);
  if (res >= 0) return res;
  return set_errno(spiffs_err_to_errno(e));
}

int spiffs_vfs_open(spiffs *fs, const char *path, int flags, int mode) {
  (void) mode;
  spiffs_mode sm = 0;
  int rw = (flags & 3);
  if (rw == O_RDONLY || rw == O_RDWR) sm |= SPIFFS_RDONLY;
  if (rw == O_WRONLY || rw == O_RDWR) sm |= SPIFFS_WRONLY;
  if (flags & O_CREAT) sm |= SPIFFS_CREAT;
  if (flags & O_TRUNC) sm |= SPIFFS_TRUNC;
#ifdef O_EXCL
  if (flags & O_EXCL) sm |= SPIFFS_EXCL;
#endif

  path = drop_dir(path);


  if (flags & O_APPEND) sm |= SPIFFS_APPEND;
  return set_spiffs_errno(fs, path, SPIFFS_open(fs, drop_dir(path), sm, 0));
}

int spiffs_vfs_close(spiffs *fs, int fd) {
  return set_spiffs_errno(fs, "close", SPIFFS_close(fs, fd));
}

ssize_t spiffs_vfs_read(spiffs *fs, int fd, void *dstv, size_t size) {
    {
        int n = SPIFFS_read(fs, fd, dstv, size);
        if (n < 0 && SPIFFS_errno(fs) == SPIFFS_ERR_END_OF_OBJECT) {
          /* EOF */
          n = 0;
        }
        return set_spiffs_errno(fs, "read", n);
    }
}

size_t spiffs_vfs_write(spiffs *fs, int fd, const void *datav, size_t size) {
    {
        return set_spiffs_errno(fs, "write",
                            SPIFFS_write(fs, fd, (void *) datav, size));
    }
}

static void spiffs_vfs_xlate_stat(spiffs_stat *ss, struct stat *st) {
  st->st_ino = ss->obj_id;
  st->st_mode = S_IFREG | 0666;
  st->st_nlink = 1;
  st->st_size = ss->size;
}

int spiffs_vfs_stat(spiffs *fs, const char *path, struct stat *st) {
  int res;
  spiffs_stat ss;
  memset(st, 0, sizeof(*st));
  const char *fname = drop_dir(path);
  /* Simulate statting the root directory. */
  if (fname[0] == '\0' || strcmp(fname, ".") == 0) {
    st->st_ino = 0;
    st->st_mode = S_IFDIR | 0777;
    st->st_nlink = 1;
    st->st_size = 0;
    return set_spiffs_errno(fs, path, SPIFFS_OK);
  }
  res = SPIFFS_stat(fs, fname, &ss);
  if (res == SPIFFS_OK) {
    spiffs_vfs_xlate_stat(&ss, st);
  }
 /* else
  {
      //Fix to check if this is a directory
      int len = strlen(path) + 1;
      //alloc temporary string for dir checking
      if(len > 2 && !(path[len - 3] == '/' && path[len - 2] == '.'))
      {
          char *dir = malloc(len + 2);
          strcpy(dir, path);
          if(path[len - 2] == '/')
          {
              dir[len - 1] = '.';
              dir[len] = '\0';
          }
          else
          {
              dir[len - 1] = '/';
              dir[len] = '.';
              dir[len + 1] = '\0';
          }
          res = spiffs_vfs_stat(fs, dir, st);
          free(dir);
      }
  }*/
  return set_spiffs_errno(fs, path, res);
}

int spiffs_vfs_fstat(spiffs *fs, int fd, struct stat *st) {
  int res;
  spiffs_stat ss;
  memset(st, 0, sizeof(*st));
  res = SPIFFS_fstat(fs, fd, &ss);
  if (res == SPIFFS_OK) {
    spiffs_vfs_xlate_stat(&ss, st);
  }
  return set_spiffs_errno(fs, "fstat", res);
}

off_t spiffs_vfs_lseek(spiffs *fs, int fd, off_t offset, int whence) {
  return set_spiffs_errno(fs, "lseek", SPIFFS_lseek(fs, fd, offset, whence));
}

int spiffs_vfs_rename(spiffs *fs, const char *src, const char *dst) {
  int res;
  src = drop_dir(src);
  dst = drop_dir(dst);
  /* Renaming file to itself should be a no-op. */
  if (strcmp(src, dst) == 0) return 0;
  {
    /*
     * POSIX rename requires that in case "to" exists, it be atomically replaced
     * with "from". The atomic part we can't do, but at least we can do replace.
     */
    spiffs_stat ss;
    res = SPIFFS_stat(fs, dst, &ss);
    if (res == 0) {
      SPIFFS_remove(fs, dst);
    }
  }
  return set_spiffs_errno(fs, "rename", SPIFFS_rename(fs, src, dst));
}

int spiffs_vfs_unlink(spiffs *fs, const char *path) {
  path = drop_dir(path);
  return set_spiffs_errno(fs, "unlink", SPIFFS_remove(fs, path));
}

struct spiffs_dir {
  DIR dir;
  spiffs_DIR sdh;
  struct spiffs_dirent sde;
  struct dirent de;
};

DIR *spiffs_vfs_opendir(spiffs *fs, const char *name) {
  struct spiffs_dir *sd = NULL;

  if (name == NULL) {
    errno = EINVAL;
    return NULL;
  }

  if ((sd = (struct spiffs_dir *) calloc(1, sizeof(*sd))) == NULL) {
    errno = ENOMEM;
    return NULL;
  }

  if (SPIFFS_opendir(fs, name, &sd->sdh) == NULL) {
    free(sd);
    sd = NULL;
    errno = EINVAL;
  }

  return (DIR *) sd;
}

struct dirent *spiffs_vfs_readdir(spiffs *fs, DIR *dir) {
  struct spiffs_dir *sd = (struct spiffs_dir *) dir;
  if (SPIFFS_readdir(&sd->sdh, &sd->sde) == SPIFFS_OK) {
    errno = EBADF;
    return NULL;
  }
  sd->de.d_ino = sd->sde.obj_id;
  memcpy(sd->de.d_name, sd->sde.name, SPIFFS_OBJ_NAME_LEN);
  (void) fs;
  return &sd->de;
}

int spiffs_vfs_closedir(spiffs *fs, DIR *dir) {
  struct spiffs_dir *sd = (struct spiffs_dir *) dir;
  if (dir != NULL) {
    SPIFFS_closedir(&sd->sdh);
    free(dir);
  }
  (void) fs;
  return 0;
}
