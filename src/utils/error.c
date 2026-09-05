#include <string.h>
#include "error.h"

const char *myrun_strerror(myrun_err_t err) {
    switch (err) {
        case MYRUN_SUCCESS:
            return "Success";
        case MYRUN_ERR_INVALID_ARG:
            return "Invalid argument or configuration";
        case MYRUN_ERR_NOMEM:
            return "Out of memory";
        case MYRUN_ERR_FORK:
            return "Failed to fork child process";
        case MYRUN_ERR_EXEC:
            return "Failed to execute target program";
        case MYRUN_ERR_WAIT:
            return "Failed to wait for child process";
        case MYRUN_ERR_SIGNAL:
            return "Failed to configure signal handlers";
        case MYRUN_ERR_CLONE:
            return "Failed to clone child process with namespaces";
        case MYRUN_ERR_NAMESPACE:
            return "Failed to configure Linux namespace";
        case MYRUN_ERR_UTS:
            return "Failed to configure UTS namespace / hostname";
        case MYRUN_ERR_NOT_FOUND:
            return "Command or executable not found";
        case MYRUN_ERR_PERMISSION:
            return "Permission denied executing command";
        case MYRUN_ERR_GENERIC:
        default:
            return "Generic / unknown error";
    }
}
