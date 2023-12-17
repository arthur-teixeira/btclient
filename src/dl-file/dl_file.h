#ifndef DL_FILE_H
#define DL_FILE_H

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

typedef struct dl_file {
  pthread_mutex_t file_lock;
  size_t size;
  uint8_t *data;
  char path[];
} dl_file_t;

typedef struct filemem {
  void *mem;
  size_t size;
} filemem_t;

int dl_file_close_and_free(dl_file_t *file);
void dl_file_getfilemem(const dl_file_t *file, filemem_t *out);
int dl_file_complete(dl_file_t *file);
dl_file_t *dl_file_create_and_open(size_t size, const char *path);

#endif // DL_FILE_H
