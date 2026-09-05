#ifndef MYRUN_LOG_H
#define MYRUN_LOG_H

#include <stdio.h>
#include <stdbool.h>

/**
 * @file log.h
 * @brief Structured, colorized logging system for MyRun.
 */

typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO  = 1,
    LOG_LEVEL_WARN  = 2,
    LOG_LEVEL_ERROR = 3,
    LOG_LEVEL_FATAL = 4,
    LOG_LEVEL_NONE  = 5
} log_level_t;

/**
 * @brief Sets the global minimum log level.
 * @param level Minimum log level to print.
 */
void log_set_level(log_level_t level);

/**
 * @brief Enables or disables colored output.
 * @param enable True to enable colors, false to disable.
 */
void log_set_color(bool enable);

/**
 * @brief Core logging function.
 */
void log_write(log_level_t level, const char *file, int line, const char *fmt, ...)
    __attribute__((format(printf, 4, 5)));

#define LOG_DEBUG(...) log_write(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)  log_write(LOG_LEVEL_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)  log_write(LOG_LEVEL_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) log_write(LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_FATAL(...) log_write(LOG_LEVEL_FATAL, __FILE__, __LINE__, __VA_ARGS__)

#endif /* MYRUN_LOG_H */
