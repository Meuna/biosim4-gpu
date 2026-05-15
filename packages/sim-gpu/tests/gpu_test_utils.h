#ifndef BIOSIM_GPU_TEST_UTILS_H
#define BIOSIM_GPU_TEST_UTILS_H

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/opencl.h>
#endif

#include "biosim/core/status.h"
#include "biosim/core/test_utils.h"
#include "biosim/sim-gpu/runner.h"

/* Creates a kernel and program for the given kernel name. */
biosim_status_t gpu_test_kernel_runtime_create(biosim_gpu_runner_t *runner, cl_program *program,
                                               cl_kernel *kernel, const char *registry_name,
                                               const char *kernel_name);

/* Convenience function for allocating memory with error checking */
void *calloc_test_assert(size_t count, size_t size);

/* malloc one buffer; bail on failure, teardown will release everything */
#define ALLOC(var, cnt, esz)                                                                       \
    (var) = malloc((size_t)(cnt) * (esz));                                                           \
    if (!(var)) {                                                                                    \
        fixture_status = BIOSIM_ERR_NOMEM;                                                         \
        return;                                                                                    \
    }

/* Create a read-only OpenCL buffer; bail on failure, teardown will release everything */
#define MKRO(var, ptr, esz, cnt)                                                                   \
    do {                                                                                           \
        cl_int cl_err;                                                                             \
        const void *mkro_ptr = (ptr);                                                              \
        cl_mem_flags flags = CL_MEM_READ_ONLY | (mkro_ptr ? CL_MEM_COPY_HOST_PTR : 0);             \
        (var) =                                                                                    \
            clCreateBuffer(runner.context, flags, (size_t)(cnt) * (esz), (void *)(ptr), &cl_err);  \
        if (cl_err != CL_SUCCESS || !(var)) {                                                      \
            fixture_status = BIOSIM_ERR_OPENCL;                                                    \
            return;                                                                                \
        }                                                                                          \
    } while (0)

/* Create a read-write OpenCL buffer; bail on failure, teardown will release everything */
#define MKRW(var, ptr, esz, cnt)                                                                   \
    do {                                                                                           \
        cl_int cl_err;                                                                             \
        const void *mkrw_ptr = (ptr);                                                              \
        cl_mem_flags flags = CL_MEM_READ_WRITE | (mkrw_ptr ? CL_MEM_COPY_HOST_PTR : 0);            \
        (var) =                                                                                    \
            clCreateBuffer(runner.context, flags, (size_t)(cnt) * (esz), (void *)(ptr), &cl_err);  \
        if (cl_err != CL_SUCCESS || !(var)) {                                                      \
            fixture_status = BIOSIM_ERR_OPENCL;                                                    \
            return;                                                                                \
        }                                                                                          \
    } while (0)

/* Create a write-only OpenCL buffer; bail on failure, teardown will release everything */
#define MKWO(var, ptr, esz, cnt)                                                                   \
    do {                                                                                           \
        cl_int cl_err;                                                                             \
        const void *mkwo_ptr = (ptr);                                                              \
        cl_mem_flags flags = CL_MEM_WRITE_ONLY | (mkwo_ptr ? CL_MEM_COPY_HOST_PTR : 0);            \
        (var) =                                                                                    \
            clCreateBuffer(runner.context, flags, (size_t)(cnt) * (esz), (void *)(ptr), &cl_err);  \
        if (cl_err != CL_SUCCESS || !(var)) {                                                      \
            fixture_status = BIOSIM_ERR_OPENCL;                                                    \
            return;                                                                                \
        }                                                                                          \
    } while (0)

/* Write data to an OpenCL buffer; bail on failure */
#define READ(buf, ptr, cnt, esz)                                                                   \
    if (clEnqueueReadBuffer(q, (buf), CL_FALSE, 0U, (size_t)(cnt) * (esz), (ptr), 0U, NULL,        \
                            NULL) != CL_SUCCESS) {                                                 \
        return 0;                                                                                  \
    }

/* Write data to an OpenCL buffer; bail on failure */
#define WRITE(buf, ptr, cnt, esz)                                                                  \
    if (clEnqueueWriteBuffer(q, (buf), CL_FALSE, 0U, (size_t)(cnt) * (esz), (ptr), 0U, NULL,       \
                             NULL) != CL_SUCCESS) {                                                \
        return 0;                                                                                  \
    }

/* Safe OpenCL buffer release. */
#define SAFE_RELEASE(fn, obj)                                                                      \
    do {                                                                                           \
        if ((obj) != NULL) {                                                                       \
            (void)(fn)(obj);                                                                       \
        }                                                                                          \
    } while (0)

#endif /* BIOSIM_GPU_TEST_UTILS_H */
