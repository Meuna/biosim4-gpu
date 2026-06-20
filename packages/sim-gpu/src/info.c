#include "biosim/sim-gpu/info.h"

#include "cl_macros.h"

/* OpenCL name/version strings are short; this buffer is comfortably oversized. */
#define INFO_STR_CAP 256U

biosim_status_t biosim_gpu_info_print(
    const biosim_params_t *p,
    const biosim_gpu_runner_t *runner,
    const char *version,
    bool verbose,
    FILE *out
) {
    biosim_status_t returncode = BIOSIM_OK;
    char platform_name[INFO_STR_CAP] = {0};
    char device_name[INFO_STR_CAP] = {0};
    char device_version[INFO_STR_CAP] = {0};

    CL_GOTO_EXIT_ON_ERROR(clGetPlatformInfo(
        runner->platform, CL_PLATFORM_NAME, sizeof(platform_name), platform_name, NULL
    ));
    CL_GOTO_EXIT_ON_ERROR(
        clGetDeviceInfo(runner->device, CL_DEVICE_NAME, sizeof(device_name), device_name, NULL)
    );
    CL_GOTO_EXIT_ON_ERROR(clGetDeviceInfo(
        runner->device, CL_DEVICE_VERSION, sizeof(device_version), device_version, NULL
    ));

    (void)fprintf(out, "version:  %s\n", version);
    (void)fprintf(out, "platform: %s\n", platform_name);
    (void)fprintf(out, "device:   %s (%s)\n", device_name, device_version);
    (void)fprintf(
        out,
        "params:   max-generations=%d population=%d max-genes=%d max-neurons=%d\n",
        biosim_params_get_int(p, "max-generations"),
        biosim_params_get_int(p, "population"),
        biosim_params_get_int(p, "max-genes"),
        biosim_params_get_int(p, "max-neurons")
    );

    if (verbose) {
        (void)fprintf(out, "all parameters:\n");
        biosim_params_dump(p, out);
    }

exit:
    if (returncode != BIOSIM_OK) {
        BIOSIM_ERRORF("biosim_gpu_info_print failed (%s)", biosim_strerror(returncode));
    }
    return returncode;
}
