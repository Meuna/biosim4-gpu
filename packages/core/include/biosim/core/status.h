#ifndef BIOSIM_CORE_STATUS_H
#define BIOSIM_CORE_STATUS_H

#include <stdio.h>

typedef enum {
    BIOSIM_OK = 0,
    BIOSIM_ERR_NOMEM,
    BIOSIM_ERR_TYPE,
    BIOSIM_ERR_NOTFOUND,
    BIOSIM_ERR_INVALID,
    BIOSIM_ERR_IO,
    BIOSIM_ERR_OPENCL,
    BIOSIM_EOF,
    BIOSIM_WARN_UNKNOWN_KEY,
} biosim_status_t;

const char *biosim_strerror(biosim_status_t code);
biosim_status_t biosim_io_status(FILE *f);

#endif /* BIOSIM_CORE_STATUS_H */
