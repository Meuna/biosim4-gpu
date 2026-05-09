#ifndef BIOSIM_SIM_GPU_CL_MACROS_H
#define BIOSIM_SIM_GPU_CL_MACROS_H

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/opencl.h>
#endif

#include "biosim/core/log.h"

#define CL_SAFE_RELEASE(fn, obj)                                                                   \
    do {                                                                                           \
        if ((obj) != NULL) {                                                                       \
            (void)(fn)(obj);                                                                       \
        }                                                                                          \
    } while (0)

#define CL_GOTO_EXIT_ON_ERROR(expr)                                                                \
    do {                                                                                           \
        cl_int _err = (expr);                                                                      \
        if (_err != CL_SUCCESS) {                                                                  \
            BIOSIM_ERRORF("%s failed (OpenCL %d)", #expr, _err);                                   \
            returncode = BIOSIM_ERR_OPENCL;                                                        \
            goto exit;                                                                             \
        }                                                                                          \
    } while (0)

#define CL_ASSIGN_OR_GOTO_EXIT(dst, expr)                                                          \
    do {                                                                                           \
        (dst) = (expr);                                                                            \
        if (cl_err != CL_SUCCESS) {                                                                \
            BIOSIM_ERRORF("%s failed (OpenCL %d)", #expr, (int)cl_err);                            \
            returncode = BIOSIM_ERR_INVALID;                                                       \
            goto exit;                                                                             \
        }                                                                                          \
    } while (0)

#endif /* BIOSIM_SIM_GPU_CL_MACROS_H */
