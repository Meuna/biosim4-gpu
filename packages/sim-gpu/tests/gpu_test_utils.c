#include "gpu_test_utils.h"
#include "biosim/sim-gpu/registry.h"
#include "unity.h"

#include <string.h>

biosim_status_t gpu_test_kernel_runtime_create(biosim_gpu_runner_t *runner, cl_program *program,
                                               cl_kernel *kernel, const char *registry_name,
                                               const char *kernel_name) {

    cl_uint n = 0U;
    if (clGetPlatformIDs(0U, NULL, &n) != CL_SUCCESS || n == 0U) {
        return BIOSIM_ERR_OPENCL;
    }

    biosim_status_t rc = biosim_gpu_runner_create(0U, 0U, runner);
    if (rc != BIOSIM_OK) {
        return rc;
    }

    biosim_gpu_kernel_sources_t sources;
    memset(&sources, 0, sizeof(sources));
    rc = biosim_gpu_registry_get(registry_name, NULL, &sources);
    if (rc != BIOSIM_OK) {
        return rc;
    }

    rc = biosim_gpu_program_build(runner, sources.sources, sources.count, program);
    biosim_gpu_kernel_sources_free(&sources);
    if (rc != BIOSIM_OK) {
        return rc;
    }

    cl_int cl_err;
    *kernel = clCreateKernel(*program, kernel_name, &cl_err);
    if (cl_err != CL_SUCCESS || !*kernel) {
        return BIOSIM_ERR_OPENCL;
    }

    return BIOSIM_OK;
}

void *calloc_test_assert(size_t count, size_t size) {
    void *ptr = calloc(count, size);
    TEST_ASSERT_NOT_NULL(ptr);
    return ptr;
}