#include "dl_file.h"
#include "../log/log.h"
#include <assert.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <sys/mman.h>

dl_file_t *dl_file_create_and_open(size_t size, const char *path) {
  char newpath[512];
  strcpy(newpath, path);
  strcat(newpath, ".incomplete");

  log_printf(LOG_DEBUG, "OPENING FILE AT %s\n", path);
  int fd = open(path, O_CREAT | O_RDWR, 0777);
  if (fd < 0) {
    goto fail_open;
  }

  if (ftruncate(fd, size) < 0) {
    goto fail_truncate;
  }

  struct stat stats;
  fstat(fd, &stats);
  assert((size_t)stats.st_size == size);

  uint8_t *mem =
      mmap(NULL, stats.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (!mem) {
    goto fail_mmap;
  }

  dl_file_t *file = malloc(sizeof(dl_file_t) + strlen(newpath) + 1);
  if (!file) {
    goto fail_alloc;
  }

  pthread_mutex_init(&file->file_lock, NULL);
  file->size = size;
  file->data = mem;
  memcpy(file->path, newpath, strlen(newpath));
  file->path[strlen(newpath)] = '\0';

  rename(path, newpath);

  close(fd);
  log_printf(LOG_INFO, "Successfully created and opened file at %s\n", path);

  return file;

fail_alloc:
  munmap(mem, stats.st_size);
fail_mmap:
fail_truncate:
  close(fd);
fail_open:
  log_printf(LOG_ERROR, "Unable to (create and) open file at %s\n", path);
  return NULL;
}

int dl_file_close_and_free(dl_file_t *file) {
  int ret = munmap(file->data, file->size);
  pthread_mutex_destroy(&file->file_lock);
  free(file);

  return ret;
}

void dl_file_getfilemem(const dl_file_t *file, filemem_t *out) {
  out->mem = file->data;
  out->size = file->size;
}

int dl_file_complete(dl_file_t *file) {
  char oldpath[512];
  strncpy(oldpath, file->path, sizeof(oldpath));
  char *trim = strstr(file->path, ".incomplete");
  assert(trim && trim > file->path);

  *trim = '\0';
  rename(oldpath, file->path);

  return 0;
}
