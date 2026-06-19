#include "biosim/sim-gpu/runner.h"
#include "biosim/core/log.h"

#include "cl_macros.h"

#include <stdlib.h>
#include <string.h>

/* ── internal helpers ───────────────────────────────────────────────────── */

static biosim_status_t log_build_info(
    const biosim_gpu_runner_t *r, cl_program program, cl_program_build_info param_name
) {
    /* alloc start here, free on exit label */
    char *log_buf = NULL;
    biosim_status_t returncode = BIOSIM_OK;
    cl_int cl_err = CL_SUCCESS;
    size_t log_size = 0U;

    CL_GOTO_EXIT_ON_ERROR(clGetProgramBuildInfo(program, r->device, param_name, 0U, NULL, &log_size)
    );
    if (log_size == 0U) {
        BIOSIM_INFOF("clGetProgramBuildInfo: build info is empty");
        returncode = BIOSIM_ERR_INVALID;
        goto exit;
    }

    log_buf = (char *)malloc(log_size);
    if (!log_buf) {
        returncode = BIOSIM_ERR_NOMEM;
        goto exit;
    }

    CL_GOTO_EXIT_ON_ERROR(
        clGetProgramBuildInfo(program, r->device, param_name, log_size, log_buf, NULL)
    );

    /* Log happen here */
    BIOSIM_INFOF("OpenCL build info:\n%s", log_buf);

exit:
    free(log_buf);
    return returncode;
}

/* ── lifecycle ──────────────────────────────────────────────────────────── */

biosim_status_t biosim_gpu_runner_create(
    uint32_t platform_idx, uint32_t device_idx, bool enable_profiling, biosim_gpu_runner_t *out
) {
    memset(out, 0, sizeof(*out));

    /* alloc start here, free on exit label */
    cl_platform_id *platforms = NULL;
    cl_device_id *devices = NULL;
    biosim_status_t returncode = BIOSIM_OK;
    cl_int cl_err = CL_SUCCESS;

    cl_uint n_platforms = 0U;
    CL_GOTO_EXIT_ON_ERROR(clGetPlatformIDs(0U, NULL, &n_platforms));
    if (n_platforms == 0U) {
        BIOSIM_ERRORF("No OpenCL platforms found");
        returncode = BIOSIM_ERR_NOTFOUND;
        goto exit;
    }
    if (platform_idx >= n_platforms) {
        BIOSIM_ERRORF("Platform index out of bounds");
        returncode = BIOSIM_ERR_NOTFOUND;
        goto exit;
    }

    platforms = (cl_platform_id *)malloc((size_t)n_platforms * sizeof(cl_platform_id));
    if (!platforms) {
        returncode = BIOSIM_ERR_NOMEM;
        goto exit;
    }

    CL_GOTO_EXIT_ON_ERROR(clGetPlatformIDs(n_platforms, platforms, NULL));
    out->platform = platforms[platform_idx];

    cl_uint n_devices = 0U;
    CL_GOTO_EXIT_ON_ERROR(clGetDeviceIDs(out->platform, CL_DEVICE_TYPE_ALL, 0U, NULL, &n_devices));
    if (n_devices == 0U) {
        BIOSIM_ERRORF("No OpenCL devices found on platform %u", platform_idx);
        returncode = BIOSIM_ERR_NOTFOUND;
        goto exit;
    }
    if (device_idx >= n_devices) {
        BIOSIM_ERRORF("Device index out of bounds");
        returncode = BIOSIM_ERR_NOTFOUND;
        goto exit;
    }

    devices = (cl_device_id *)malloc((size_t)n_devices * sizeof(cl_device_id));
    if (!devices) {
        returncode = BIOSIM_ERR_NOMEM;
        goto exit;
    }

    CL_GOTO_EXIT_ON_ERROR(
        clGetDeviceIDs(out->platform, CL_DEVICE_TYPE_ALL, n_devices, devices, NULL)
    );
    out->device = devices[device_idx];

    CL_ASSIGN_OR_GOTO_EXIT(
        out->context, clCreateContext(NULL, 1U, &out->device, NULL, NULL, &cl_err)
    );

    const cl_queue_properties profiling_props[] = {
        CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0
    };
    const cl_queue_properties *queue_props = enable_profiling ? profiling_props : NULL;
    CL_ASSIGN_OR_GOTO_EXIT(
        out->queue,
        clCreateCommandQueueWithProperties(out->context, out->device, queue_props, &cl_err)
    );
    out->profiling = enable_profiling;

exit:
    free((void *)platforms);
    free((void *)devices);
    if (returncode != BIOSIM_OK) {
        biosim_gpu_runner_free(out);
        BIOSIM_ERRORF("create failed (%s)", biosim_strerror(returncode));
    }
    return returncode;
}

void biosim_gpu_runner_free(biosim_gpu_runner_t *r) {
    if (!r) {
        return;
    }
    CL_SAFE_RELEASE(clReleaseCommandQueue, r->queue);
    CL_SAFE_RELEASE(clReleaseContext, r->context);
    memset(r, 0, sizeof(*r));
}

/* ── program compilation ────────────────────────────────────────────────── */

biosim_status_t biosim_gpu_program_build(
    const biosim_gpu_runner_t *r, const char **sources, size_t n_sources, cl_program *out
) {
    cl_int cl_err = CL_SUCCESS;
    cl_program program =
        clCreateProgramWithSource(r->context, (cl_uint)n_sources, sources, NULL, &cl_err);
    if (cl_err != CL_SUCCESS) {
        BIOSIM_ERRORF("clCreateProgramWithSource failed (OpenCL error %d)", (int)cl_err);
        return BIOSIM_ERR_OPENCL;
    }

    cl_err = clBuildProgram(program, 1U, &r->device, NULL, NULL, NULL);
    if (cl_err != CL_SUCCESS) {
        (void)log_build_info(r, program, CL_PROGRAM_BUILD_LOG);
        (void)clReleaseProgram(program);
        BIOSIM_ERRORF("clBuildProgram failed (OpenCL error %d)", (int)cl_err);
        return BIOSIM_ERR_OPENCL;
    }

    *out = program;

    return BIOSIM_OK;
}
