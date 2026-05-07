#include "biosim/core/status.h"

const char *biosim_strerror(biosim_status_t code) {
    switch (code) {
    case BIOSIM_OK:
        return "ok";
    case BIOSIM_ERR_NOMEM:
        return "out of memory";
    case BIOSIM_ERR_TYPE:
        return "type mismatch";
    case BIOSIM_ERR_NOTFOUND:
        return "not found";
    case BIOSIM_ERR_INVALID:
        return "invalid data or configuration";
    case BIOSIM_ERR_IO:
        return "I/O failure";
    case BIOSIM_WARN_UNKNOWN_KEY:
        return "unknown configuration key";
    }
    return "unknown error";
}
