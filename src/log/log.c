#include "log.h"

// #include "pthread.h"
#include "stdarg.h"
#include <stdio.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

// pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;
log_level_t level = DEFAULT_LOG_LVL;
FILE *log_file = NULL;

void log_set_lvl(log_level_t lvl) {
  //  pthread_mutex_lock(&log_lock);
  level = lvl;
  // pthread_mutex_unlock(&log_lock);
}

void log_set_logfile(FILE *f) {
  // pthread_mutex_lock(&log_lock);
  log_file = f;
  // pthread_mutex_unlock(&log_lock);
}

void log_printf(log_level_t lvl, const char *fmt, ...) {
  va_list args;
  time_t now = time(0);
  long tid = (long)syscall(SYS_gettid);

  char timestr[9];
  strftime(timestr, sizeof(timestr), "%H:%M:%S", localtime(&now));

  if (lvl < level) {
    return;
  }

  // pthread_mutex_lock(&log_lock);

  fprintf(log_file, "[%.*s] [%05ld] ", 8, timestr, tid);
  switch (lvl) {
  case LOG_WARNING:
    fprintf(log_file, "WARNING: ");
    break;
  case LOG_ERROR:
    fprintf(log_file, "ERROR: ");
    break;
  default:
    break;
  }

  va_start(args, fmt);
  vfprintf(log_file, fmt, args);
  va_end(args);

  // pthread_mutex_unlock(&log_lock);

  fflush(log_file);
}
