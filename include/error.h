#ifndef MYRUN_ERROR_H
#define MYRUN_ERROR_H

#include <errno.h>

/**
 * @file error.h
 * @brief Error codes and error handling utilities for MyRun.
 */

typedef enum {
    MYRUN_SUCCESS = 0,
    MYRUN_ERR_INVALID_ARG = 1,
    MYRUN_ERR_NOMEM = 2,
    MYRUN_ERR_FORK = 3,
    MYRUN_ERR_EXEC = 4,
    MYRUN_ERR_WAIT = 5,
    MYRUN_ERR_SIGNAL = 6,
    MYRUN_ERR_CLONE = 7,
    MYRUN_ERR_NAMESPACE = 8,
    MYRUN_ERR_UTS = 9,
    MYRUN_ERR_MOUNT = 10,
    MYRUN_ERR_NOT_FOUND = 127,
    MYRUN_ERR_PERMISSION = 126,
    MYRUN_ERR_GENERIC = 255
} myrun_err_t;

/**
 * @brief Returns a human-readable string for a MyRun error code.
 * @param err MyRun error code
 * @return String description of the error
 */
const char *myrun_strerror(myrun_err_t err);

#endif /* MYRUN_ERROR_H */
