#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>

#include "uts.h"
#include "log.h"
#include "error.h"

int hostname_validate(const char *hostname) {
    if (!hostname) {
        LOG_ERROR("Hostname cannot be NULL");
        return MYRUN_ERR_INVALID_ARG;
    }

    size_t len = strlen(hostname);
    if (len == 0) {
        LOG_ERROR("Hostname cannot be empty");
        return MYRUN_ERR_INVALID_ARG;
    }

    if (len > MYRUN_MAX_HOSTNAME_LEN) {
        LOG_ERROR("Hostname '%s' exceeds maximum allowed length (%d chars)",
                  hostname, MYRUN_MAX_HOSTNAME_LEN);
        return MYRUN_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)hostname[i];
        if (c < 32 || c == 127) {
            LOG_ERROR("Hostname contains invalid control character (code %u) at index %zu", c, i);
            return MYRUN_ERR_INVALID_ARG;
        }
        if (isspace(c)) {
            LOG_ERROR("Hostname cannot contain whitespace");
            return MYRUN_ERR_INVALID_ARG;
        }
        if (c == '/') {
            LOG_ERROR("Hostname cannot contain '/'");
            return MYRUN_ERR_INVALID_ARG;
        }
        if (!isalnum(c) && c != '-' && c != '.' && c != '_') {
            LOG_ERROR("Hostname contains disallowed character '%c' at index %zu", c, i);
            return MYRUN_ERR_INVALID_ARG;
        }
    }

    return MYRUN_SUCCESS;
}

int uts_namespace_setup(const char *hostname) {
    if (!hostname) {
        return MYRUN_SUCCESS;
    }

    int valid_err = hostname_validate(hostname);
    if (valid_err != MYRUN_SUCCESS) {
        return valid_err;
    }

    size_t len = strlen(hostname);

#ifdef __linux__
    if (sethostname(hostname, len) < 0) {
        LOG_ERROR("sethostname() failed: %s (errno %d)", strerror(errno), errno);
        return MYRUN_ERR_UTS;
    }
    LOG_DEBUG("Successfully configured UTS hostname to '%s'", hostname);
    return MYRUN_SUCCESS;
#else
    (void)len;
    LOG_WARN("sethostname() is only supported on Linux UTS namespaces; skipped on host");
    return MYRUN_SUCCESS;
#endif
}
