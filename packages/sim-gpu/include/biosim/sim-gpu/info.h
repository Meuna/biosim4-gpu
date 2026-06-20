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
 * Print a startup info dump to out:
 *   - the version string,
 *   - the OpenCL platform and device the runner selected,
 *   - the key performance params (max-generations, population, max-genes,
 *     max-neurons).
 * When verbose, additionally lists every parameter value.
 *
 * Returns:
 *   BIOSIM_OK         — the dump was written
 *   BIOSIM_ERR_OPENCL — an OpenCL platform/device query failed
 */
biosim_status_t biosim_gpu_info_print(
    const biosim_params_t *p,
    const biosim_gpu_runner_t *runner,
    const char *version,
    bool verbose,
    FILE *out
);

#endif /* BIOSIM_SIM_GPU_INFO_H */
