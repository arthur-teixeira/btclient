#ifndef LOG_H
#define LOG_H

#include <stdio.h>
typedef enum{
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR,
    LOG_NONE
}log_level_t;

#define DEFAULT_LOG_LVL LOG_INFO

void log_set_lvl(log_level_t lvl);
void log_set_logfile(FILE *f);
void log_printf(log_level_t lvl, const char *fmt, ...);

#endif // LOG_H
