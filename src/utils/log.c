#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include "log.h"

static log_level_t g_log_level = LOG_LEVEL_INFO;
static int g_color_enabled = -1; /* -1 = auto-detect */

void log_set_level(log_level_t level) {
    g_log_level = level;
}

void log_set_color(bool enable) {
    g_color_enabled = enable ? 1 : 0;
}

static const char *get_level_name(log_level_t level) {
    switch (level) {
        case LOG_LEVEL_DEBUG: return "DEBUG";
        case LOG_LEVEL_INFO:  return "INFO ";
        case LOG_LEVEL_WARN:  return "WARN ";
        case LOG_LEVEL_ERROR: return "ERROR";
        case LOG_LEVEL_FATAL: return "FATAL";
        default:              return "UNKWN";
    }
}

static const char *get_level_color(log_level_t level) {
    switch (level) {
        case LOG_LEVEL_DEBUG: return "\033[36m";       /* Cyan */
        case LOG_LEVEL_INFO:  return "\033[32m";       /* Green */
        case LOG_LEVEL_WARN:  return "\033[33m";       /* Yellow */
        case LOG_LEVEL_ERROR: return "\033[31m";       /* Red */
        case LOG_LEVEL_FATAL: return "\033[1;31m";     /* Bold Red */
        default:              return "\033[0m";
    }
}

void log_write(log_level_t level, const char *file, int line, const char *fmt, ...) {
    if (level < g_log_level || level >= LOG_LEVEL_NONE) {
        return;
    }

    if (g_color_enabled == -1) {
        g_color_enabled = isatty(STDERR_FILENO) ? 1 : 0;
    }

    /* Timestamp */
    time_t raw_time = time(NULL);
    struct tm tm_buf;
    localtime_r(&raw_time, &tm_buf);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", &tm_buf);

    /* Trim file path to basename */
    const char *basename = file;
    for (const char *p = file; *p; p++) {
        if (*p == '/' && *(p + 1)) {
            basename = p + 1;
        }
    }

    /* Output header */
    if (g_color_enabled) {
        fprintf(stderr, "\033[90m[%s]\033[0m %s[%s]\033[0m \033[90m[%s:%d]\033[0m ",
                time_str, get_level_color(level), get_level_name(level), basename, line);
    } else {
        fprintf(stderr, "[%s] [%s] [%s:%d] ",
                time_str, get_level_name(level), basename, line);
    }

    /* Output message */
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, "\n");
    fflush(stderr);
}
