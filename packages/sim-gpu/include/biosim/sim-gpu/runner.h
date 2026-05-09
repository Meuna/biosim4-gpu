/*
 * HOST-ONLY: wraps OpenCL objects. Do NOT include from kernel sources.
 */
#ifndef BIOSIM_SIM_GPU_RUNNER_H
#define BIOSIM_SIM_GPU_RUNNER_H

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/opencl.h>
#endif

#include "biosim/core/log.h"
#include "biosim/core/status.h"
#include <stddef.h>
#include <stdint.h>

/* ── OpenCL runner ────────────────────────────────────────────────────────── */

/*
 * Holds the OpenCL platform, device, context, and command queue for one device.
 * Zero-initialise before use; biosim_gpu_runner_free tolerates a zero struct.
 */
typedef struct {
    cl_platform_id platform;
    cl_device_id device;
    cl_context context;
    cl_command_queue queue;
} biosim_gpu_runner_t;

/*
 * Create a runner for the device at (platform_idx, device_idx).
 *
 * Returns:
 *   BIOSIM_OK           — *out is valid; call biosim_gpu_runner_free when done
 *   BIOSIM_ERR_NOTFOUND — no platform or device at the given index
 *   BIOSIM_ERR_INVALID  — OpenCL context or queue creation failed
 */
biosim_status_t biosim_gpu_runner_create(uint32_t platform_idx, uint32_t device_idx,
                                         biosim_gpu_runner_t *out);

/* Release all OpenCL objects. Safe to call on a zero-initialised struct. */
void biosim_gpu_runner_free(biosim_gpu_runner_t *r);

/* ── program compilation ──────────────────────────────────────────────────── */

/*
 * Compile an OpenCL program from n_sources source strings.
 * On build failure, the OpenCL build log is emitted to runner->log at ERROR level.
 *
 * Returns:
 *   BIOSIM_OK         — *out is a valid cl_program (caller must clReleaseProgram)
 *   BIOSIM_ERR_NOMEM  — clCreateProgramWithSource reported CL_OUT_OF_HOST_MEMORY
 *   BIOSIM_ERR_INVALID — compilation failed
 */
biosim_status_t biosim_gpu_program_build(const biosim_gpu_runner_t *r, const char **sources,
                                         size_t n_sources, cl_program *out);

#endif /* BIOSIM_SIM_GPU_RUNNER_H */
