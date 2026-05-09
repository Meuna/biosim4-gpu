#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/opencl.h>
#endif

#include "biosim/core/log.h"
#include "biosim/core/rng.h"
#include "biosim/core/sim.h"
#include "biosim/core/status.h"
#include "biosim/params/params.h"
#include "biosim/sim-gpu/registry.h"
#include "biosim/sim-gpu/runner.h"

#include "cl_macros.h"

static volatile sig_atomic_t g_halt_requested = 0;

static void handle_signal(int sig) {
    (void)sig;
    g_halt_requested = 1;
}

/* Derive the directory component of a path, storing it into buf.
 * If path has no separator, buf receives ".". */
static void path_dirname(const char *path, char *buf, size_t bufsize) {
    strncpy(buf, path, bufsize - 1U);
    buf[bufsize - 1U] = '\0';

    char *sep = strrchr(buf, '/');
#ifdef _WIN32
    char *sep2 = strrchr(buf, '\\');
    if (sep2 > sep) {
        sep = sep2;
    }
#endif
    if (sep) {
        *sep = '\0';
    } else {
        buf[0] = '.';
        buf[1] = '\0';
    }
}

// clang-format off
static const biosim_param_entry_t sim_params[] = {
    {"verbose",         NULL,        {.i = 0},    PARAM_COUNT, false, false, "verbose",       "v"},
    {"population",      "simulation",{.i = 1024}, PARAM_INT,   false, true,  "pop",           "p"},
    {"grid-size-x",     "simulation",{.i = 64},   PARAM_INT,   false, true,  "grid-size-x",   "x"},
    {"grid-size-y",     "simulation",{.i = 64},   PARAM_INT,   false, true,  "grid-size-y",   "y"},
    {"steps-per-gen",   "simulation",{.i = 300},  PARAM_INT,   false, true,  "steps-per-gen", NULL},
    {"max-generations", "simulation",{.i = 100},  PARAM_INT,   false, true,  "max-gen",       NULL},
    {"max-genome-len",  "genome",    {.i = 24},   PARAM_INT,   false, true,  "max-genome-len",NULL},
    {"max-neurons",     "genome",    {.i = 5},    PARAM_INT,   false, true,  "max-neurons",   NULL},
    {"platform-index",  "opencl",    {.i = 0},    PARAM_INT,   false, true,  "platform",      NULL},
    {"device-index",    "opencl",    {.i = 0},    PARAM_INT,   false, true,  "device",        NULL},
};
// clang-format on
#define SIM_PARAMS_COUNT (sizeof(sim_params) / sizeof(sim_params[0]))

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
int main(int argc, char **argv) {
    /* alloc start here, free on exit label */
    biosim_params_t p;
    biosim_sim_t sim;
    biosim_gpu_runner_t runner;
    biosim_gpu_kernel_sources_t sources;
    cl_program program = NULL;
    cl_kernel kernel = NULL;
    cl_mem buf_locx = NULL;
    cl_mem buf_locy = NULL;
    cl_mem buf_dir = NULL;
    cl_mem buf_rng = NULL;
    cl_mem buf_out = NULL;
    float *host_out = NULL;
    biosim_status_t returncode = BIOSIM_OK;

    memset(&sim, 0, sizeof(sim));
    memset(&runner, 0, sizeof(runner));
    memset(&sources, 0, sizeof(sources));
    biosim_log_init(&biosim_log_default_ctx);

    (void)signal(SIGINT, handle_signal);
    (void)signal(SIGTERM, handle_signal);
#ifdef _WIN32
    (void)signal(SIGBREAK, handle_signal);
#endif

    returncode = biosim_params_init(&p, sim_params, SIM_PARAMS_COUNT);
    if (returncode != BIOSIM_OK) {
        goto exit;
    }

    returncode = biosim_params_parse(&p, BIOSIM_PROGNAME, BIOSIM_GIT_VERSION, argc, argv);
    if (returncode != BIOSIM_OK) {
        goto exit;
    }

    int verbosity = biosim_params_get_int(&p, "verbose");
    if (verbosity == 1) {
        biosim_log_default_ctx.threshold = BIOSIM_LOG_INFO;
    } else if (verbosity >= 2) {
        biosim_log_default_ctx.threshold = BIOSIM_LOG_DEBUG;
    }

    sim.population = (uint32_t)biosim_params_get_int(&p, "population");
    sim.size_x = (int16_t)biosim_params_get_int(&p, "grid-size-x");
    sim.size_y = (int16_t)biosim_params_get_int(&p, "grid-size-y");
    sim.genome_max_len = (uint16_t)biosim_params_get_int(&p, "max-genome-len");
    sim.max_neurons = (uint8_t)biosim_params_get_int(&p, "max-neurons");
    sim.steps_per_gen = (uint32_t)biosim_params_get_int(&p, "steps-per-gen");
    sim.max_generations = (uint32_t)biosim_params_get_int(&p, "max-generations");
    sim.long_probe_dist = 16U;
    sim.gen_rng = biosim_rng_seed(0U, 1U);

    returncode = biosim_sim_create(&sim, NULL, 0U);
    if (returncode != BIOSIM_OK) {
        BIOSIM_ERRORF("sim create failed (%s)", biosim_strerror(returncode));
        goto exit;
    }

    uint32_t platform_idx = (uint32_t)biosim_params_get_int(&p, "platform-index");
    uint32_t device_idx = (uint32_t)biosim_params_get_int(&p, "device-index");

    returncode = biosim_gpu_runner_create(platform_idx, device_idx, &runner);
    if (returncode != BIOSIM_OK) {
        goto exit;
    }

    char exec_dir[4096];
    path_dirname(argv[0], exec_dir, sizeof(exec_dir));

    returncode = biosim_gpu_registry_get("k1_sensors", exec_dir, &sources);
    if (returncode != BIOSIM_OK) {
        goto exit;
    }

    returncode = biosim_gpu_program_build(&runner, sources.sources, sources.count, &program);
    if (returncode != BIOSIM_OK) {
        goto exit;
    }

    cl_int cl_err = CL_SUCCESS;
    CL_ASSIGN_OR_GOTO_EXIT(kernel, clCreateKernel(program, "k_sensor_eval", &cl_err));

    uint32_t pop = sim.population;
    size_t pop_bytes_s16 = (size_t)pop * sizeof(int16_t);
    size_t pop_bytes_u8 = (size_t)pop * sizeof(uint8_t);
    size_t pop_bytes_u64 = (size_t)pop * sizeof(uint64_t);
    size_t pop_bytes_f32 = (size_t)pop * sizeof(float);

    CL_ASSIGN_OR_GOTO_EXIT(buf_locx,
                           clCreateBuffer(runner.context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                          pop_bytes_s16, sim.agents.loc_x, &cl_err));

    CL_ASSIGN_OR_GOTO_EXIT(buf_locy,
                           clCreateBuffer(runner.context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                          pop_bytes_s16, sim.agents.loc_y, &cl_err));

    CL_ASSIGN_OR_GOTO_EXIT(buf_dir,
                           clCreateBuffer(runner.context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                          pop_bytes_u8, sim.agents.last_move_dir, &cl_err));

    CL_ASSIGN_OR_GOTO_EXIT(buf_rng,
                           clCreateBuffer(runner.context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                          pop_bytes_u64, sim.agents.rng_state, &cl_err));

    CL_ASSIGN_OR_GOTO_EXIT(buf_out, clCreateBuffer(runner.context, CL_MEM_WRITE_ONLY, pop_bytes_f32,
                                                   NULL, &cl_err));

    host_out = malloc(pop_bytes_f32);
    if (!host_out) {
        returncode = BIOSIM_ERR_NOMEM;
        goto exit;
    }

    cl_int size_x = (cl_int)sim.size_x;
    cl_int size_y = (cl_int)sim.size_y;
    cl_uint step = (cl_uint)sim.step;
    cl_uint steps_gen = (cl_uint)sim.steps_per_gen;
    cl_int sensor_id = (cl_int)0; /* LOC_X */

    (void)clSetKernelArg(kernel, 0U, sizeof(cl_mem), (const void *)&buf_locx);
    (void)clSetKernelArg(kernel, 1U, sizeof(cl_mem), (const void *)&buf_locy);
    (void)clSetKernelArg(kernel, 2U, sizeof(cl_mem), (const void *)&buf_dir);
    (void)clSetKernelArg(kernel, 3U, sizeof(cl_mem), (const void *)&buf_rng);
    (void)clSetKernelArg(kernel, 4U, sizeof(cl_int), &size_x);
    (void)clSetKernelArg(kernel, 5U, sizeof(cl_int), &size_y);
    (void)clSetKernelArg(kernel, 6U, sizeof(cl_uint), &step);
    (void)clSetKernelArg(kernel, 7U, sizeof(cl_uint), &steps_gen);
    (void)clSetKernelArg(kernel, 8U, sizeof(cl_int), &sensor_id);
    (void)clSetKernelArg(kernel, 9U, sizeof(cl_mem), (const void *)&buf_out);

    size_t global_size = (size_t)pop;
    CL_GOTO_EXIT_ON_ERROR(
        clEnqueueNDRangeKernel(runner.queue, kernel, 1U, NULL, &global_size, NULL, 0U, NULL, NULL));

    CL_GOTO_EXIT_ON_ERROR(clEnqueueReadBuffer(runner.queue, buf_out, CL_TRUE, 0U, pop_bytes_f32,
                                              host_out, 0U, NULL, NULL));

    printf("biosim-gpu: sensor LOC_X evaluated for %u agents (first = %.4f)\n", pop,
           (double)host_out[0]);

exit:
    free(host_out);
    CL_SAFE_RELEASE(clReleaseMemObject, buf_out);
    CL_SAFE_RELEASE(clReleaseMemObject, buf_rng);
    CL_SAFE_RELEASE(clReleaseMemObject, buf_dir);
    CL_SAFE_RELEASE(clReleaseMemObject, buf_locy);
    CL_SAFE_RELEASE(clReleaseMemObject, buf_locx);
    CL_SAFE_RELEASE(clReleaseKernel, kernel);
    CL_SAFE_RELEASE(clReleaseProgram, program);
    biosim_gpu_kernel_sources_free(&sources);
    biosim_gpu_runner_free(&runner);
    biosim_sim_free(&sim);
    biosim_params_free(&p);

    if (returncode != BIOSIM_OK) {
        BIOSIM_ERRORF("biosim-gpu exiting with error (%s)", biosim_strerror(returncode));
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
