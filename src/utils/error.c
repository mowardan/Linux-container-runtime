#include "error.h"

const char *myrun_strerror(myrun_err_t err) {
    switch (err) {
        case MYRUN_SUCCESS:
            return "Success";
        case MYRUN_ERR_INVALID_ARG:
            return "Invalid argument";
        case MYRUN_ERR_NOMEM:
            return "Out of memory";
        case MYRUN_ERR_FORK:
            return "Process creation (fork) failed";
        case MYRUN_ERR_EXEC:
            return "Command execution (execve) failed";
        case MYRUN_ERR_WAIT:
            return "Process wait failed";
        case MYRUN_ERR_SIGNAL:
            return "Signal handling error";
        case MYRUN_ERR_CLONE:
            return "Namespace clone failed";
        case MYRUN_ERR_NAMESPACE:
            return "Namespace configuration error";
        case MYRUN_ERR_UTS:
            return "UTS namespace setup failed";
        case MYRUN_ERR_MOUNT:
            return "Mount namespace setup failed";
        case MYRUN_ERR_CGROUP:
            return "cgroup setup or limit enforcement failed";
        case MYRUN_ERR_NETWORK:
            return "Network configuration failed";
        case MYRUN_ERR_SECURITY:
            return "Security constraints enforcement failed";
        case MYRUN_ERR_OCI:
            return "OCI bundle error";
        case MYRUN_ERR_NOT_FOUND:
            return "Executable or resource not found";
        case MYRUN_ERR_PERMISSION:
            return "Permission denied";
        case MYRUN_ERR_GENERIC:
        default:
            return "Generic / unknown error";
    }
}
