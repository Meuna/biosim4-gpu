/*
 * HOST-ONLY: includes <stdio.h> and OpenCL (via the implementation). Do NOT
 * include from kernel sources (.cl files).
 *
 * Reusable startup info dump for the GPU binaries (the benchmark today; the
 * biosim-gpu simulator can adopt it later).
 */
#ifndef BIOSIM_SIM_GPU_INFO_H
#define BIOSIM_SIM_GPU_INFO_H

#include "biosim/core/params.h"
#include "biosim/core/status.h"
#include "biosim/sim-gpu/runner.h"

#include <stdbool.h>
#include <stdio.h>

/*
 * Print a startup info dump the OpenCL platform and device the runner selected,
 *
 * Returns:
 *   BIOSIM_OK         — the dump was written
 *   BIOSIM_ERR_OPENCL — an OpenCL platform/device query failed
 */
biosim_status_t biosim_gpu_info_print(const biosim_gpu_runner_t *runner, FILE *out);

#endif /* BIOSIM_SIM_GPU_INFO_H */
