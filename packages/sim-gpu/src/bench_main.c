/*
 * biosim-gpu-bench — developer benchmark for the OpenCL pipeline.
 *
 * Runs the GPU pipeline (sync_from_host -> K1..K5 x steps -> sync_to_host) over
 * a fixed population with profiling enabled, then reports per-kernel GPU time,
 * host<->device transfer time, and throughput.  Uses large-brain defaults to
 * stress the feedforward kernel.  Not installed; this is a dev-only tool.
 *
 * _POSIX_C_SOURCE (for clock_gettime/CLOCK_MONOTONIC) is defined on the compiler
 * command line by CMake, keeping the source free of reserved-identifier macros.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

#include "biosim/cfgparse/challenges.h"
#include "biosim/core/log.h"
#include "biosim/core/params.h"
#include "biosim/core/sim.h"
#include "biosim/core/status.h"
#include "biosim/core/terminal.h"
#include "biosim/sim-gpu/benchmark.h"
#include "biosim/sim-gpu/pipeline.h"
#include "biosim/sim-gpu/runner.h"

// clang-format off
static const biosim_param_entry_t bench_params[] = {
    {"verbose",                   NULL,         {.i = 0},         PARAM_COUNT,  false, false, "verbose",        "v"},
    {"gens",                      "benchmark",  {.i = 20},        PARAM_INT,    false, true,  "gens",           NULL},
    {"warmup",                    "benchmark",  {.i = 2},         PARAM_INT,    false, true,  "warmup",         NULL},
    {"population",                "simulation", {.i = 8192},      PARAM_INT,    false, true,  "pop",            "p"},
    {"grid-size-x",               "simulation", {.i = 256},       PARAM_INT,    false, true,  "grid-size-x",    "x"},
    {"grid-size-y",               "simulation", {.i = 256},       PARAM_INT,    false, true,  "grid-size-y",    "y"},
    {"steps-per-gen",             "simulation", {.i = 300},       PARAM_INT,    false, true,  "steps-per-gen",  NULL},
    {"max-generations",           "simulation", {.i = 1000},      PARAM_INT,    false, true,  "max-gen",        NULL},
    {"max-genes",                 "genome",     {.i = 64},        PARAM_INT,    false, true,  "max-genes",      NULL},
    {"max-neurons",               "genome",     {.i = 32},        PARAM_INT,    false, true,  "max-neurons",    NULL},
    {"point-mutation-rate",       "genome",     {.f = 0.001},     PARAM_FLOAT,  false, true,  "point-mut-rate", NULL},
    {"sexual-reproduction",       "genome",     {.b = false},     PARAM_BOOL,   false, true,  NULL,             NULL},
    {"choose-parents-by-fitness", "genome",     {.b = false},     PARAM_BOOL,   false, true,  NULL,             NULL},
    {"los-range",                 "sensors",    {.i = 16},        PARAM_INT,    false, true,  NULL,             NULL},
    {"sensor-radius",             "sensors",    {.i = 2},         PARAM_INT,    false, true,  NULL,             NULL},
    {"enable-kill",               "actions",    {.b = false},     PARAM_BOOL,   false, true,  "enable-kill",    NULL},
    {"responsiveness-curve-k",    "actions",    {.f = 2.0F},      PARAM_FLOAT,  false, true,  NULL,             NULL},
    {"kind",                      "challenge",  {.s = "x_band"},  PARAM_STRING, false, true,  NULL,             NULL},
    {"x-min",                     "challenge",  {.f = 0.5},       PARAM_FLOAT,  false, true,  NULL,             NULL},
    {"x-max",                     "challenge",  {.f = 1.0},       PARAM_FLOAT,  false, true,  NULL,             NULL},
    {"mirror",                    "challenge",  {.b = false},     PARAM_BOOL,   false, true,  NULL,             NULL},
    {"x",                         "challenge",  {.f = 0.5},       PARAM_FLOAT,  false, true,  NULL,             NULL},
    {"y",                         "challenge",  {.f = 0.5},       PARAM_FLOAT,  false, true,  NULL,             NULL},
    {"radius",                    "challenge",  {.f = 0.333},     PARAM_FLOAT,  false, true,  NULL,             NULL},
    {"weighted",                  "challenge",  {.b = true},      PARAM_BOOL,   false, true,  NULL,             NULL},
    {"min-n",                     "challenge",  {.f = 5.0},       PARAM_FLOAT,  false, true,  NULL,             NULL},
    {"max-n",                     "challenge",  {.f = 8.0},       PARAM_FLOAT,  false, true,  NULL,             NULL},
    {"exclude-border",            "challenge",  {.b = false},     PARAM_BOOL,   false, true,  NULL,             NULL},
    {"outer-r",                   "challenge",  {.f = 0.25},      PARAM_FLOAT,  false, true,  NULL,             NULL},
    {"inner-r",                   "challenge",  {.f = 0.012},     PARAM_FLOAT,  false, true,  NULL,             NULL},
    {"platform-index",            "opencl",     {.i = 0},         PARAM_INT,    false, true,  "platform",       NULL},
    {"device-index",              "opencl",     {.i = 0},         PARAM_INT,    false, true,  "device",         NULL},
};
// clang-format on
#define BENCH_PARAMS_COUNT (sizeof(bench_params) / sizeof(bench_params[0]))

static uint64_t now_ns(void) {
#ifdef _WIN32
    LARGE_INTEGER freq;
    LARGE_INTEGER ctr;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&ctr);
    return (uint64_t)((double)ctr.QuadPart * 1.0e9 / (double)freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

/* Run `gens` full generations of the GPU pipeline (upload, step loop, download)
 * without CPU-side genetics, isolating the pipeline cost. */
static biosim_status_t run_generations(
    biosim_gpu_pipeline_t *pipeline, biosim_sim_t *sim, uint32_t gens
) {
    for (uint32_t g = 0U; g < gens; g++) {
        biosim_status_t rc = biosim_gpu_pipeline_sync_from_host(pipeline, sim);
        if (rc != BIOSIM_OK) {
            return rc;
        }
        for (uint32_t s = 0U; s < sim->steps_per_gen; s++) {
            sim->step = s;
            rc = biosim_gpu_pipeline_step(pipeline, sim);
            if (rc != BIOSIM_OK) {
                return rc;
            }
        }
        rc = biosim_gpu_pipeline_sync_to_host(pipeline, sim);
        if (rc != BIOSIM_OK) {
            return rc;
        }
    }
    return BIOSIM_OK;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
int main(int argc, char **argv) {
    /* alloc start here, free on exit label */
    biosim_params_t p;
    biosim_sim_t sim;
    biosim_gpu_runner_t runner;
    biosim_gpu_pipeline_t pipeline;
    biosim_challenge_spec_t challenge;
    biosim_status_t returncode = BIOSIM_OK;

    memset(&sim, 0, sizeof(sim));
    memset(&runner, 0, sizeof(runner));
    memset(&pipeline, 0, sizeof(pipeline));
    biosim_term_init();
    biosim_log_init(&biosim_log_default_ctx);

    returncode = biosim_params_init(&p, bench_params, BENCH_PARAMS_COUNT);
    if (returncode != BIOSIM_OK) {
        goto exit;
    }

    char version_buf[256];
    (void)snprintf(
        version_buf,
        sizeof(version_buf),
        "%s (%s, %s)",
        BIOSIM_GIT_VERSION,
        BIOSIM_BUILD_TYPE,
        BIOSIM_BUILD_TIMESTAMP
    );

    returncode = biosim_params_parse(&p, BIOSIM_PROGNAME, version_buf, argc, argv);
    if (returncode != BIOSIM_OK) {
        goto exit;
    }

    int verbosity = biosim_params_get_int(&p, "verbose");
    if (verbosity == 1) {
        biosim_log_default_ctx.threshold = BIOSIM_LOG_INFO;
    } else if (verbosity >= 2) {
        biosim_log_default_ctx.threshold = BIOSIM_LOG_DEBUG;
    }

    returncode = biosim_challenge_spec_from_params(&p, &challenge);
    if (returncode != BIOSIM_OK) {
        goto exit;
    }

    returncode = biosim_sim_create(&sim, &p, &challenge, NULL, 0U);
    if (returncode != BIOSIM_OK) {
        goto exit;
    }

    uint32_t platform_idx = (uint32_t)biosim_params_get_int(&p, "platform-index");
    uint32_t device_idx = (uint32_t)biosim_params_get_int(&p, "device-index");
    returncode = biosim_gpu_runner_create(platform_idx, device_idx, true, &runner);
    if (returncode != BIOSIM_OK) {
        goto exit;
    }

    returncode = biosim_gpu_pipeline_create(&sim, &runner, NULL, &pipeline);
    if (returncode != BIOSIM_OK) {
        goto exit;
    }

    uint32_t gens = (uint32_t)biosim_params_get_int(&p, "gens");
    uint32_t warmup = (uint32_t)biosim_params_get_int(&p, "warmup");

    /* Warm-up generations are not timed (driver JIT, first-touch allocations). */
    returncode = run_generations(&pipeline, &sim, warmup);
    if (returncode != BIOSIM_OK) {
        goto exit;
    }

    biosim_gpu_pipeline_reset_profile(&pipeline);
    uint64_t wall_start = now_ns();
    returncode = run_generations(&pipeline, &sim, gens);
    uint64_t wall_ns = now_ns() - wall_start;
    if (returncode != BIOSIM_OK) {
        goto exit;
    }

    biosim_gpu_profile_t profile;
    biosim_gpu_pipeline_get_profile(&pipeline, &profile);

    biosim_gpu_bench_metrics_t metrics;
    biosim_gpu_bench_compute(&profile, wall_ns, sim.population, sim.steps_per_gen, gens, &metrics);

    (void)printf(
        "=== GPU benchmark (pop=%u genes=%u neurons=%u, gens=%u) ===\n",
        sim.population,
        (unsigned)sim.max_genes,
        (unsigned)sim.max_neurons,
        gens
    );
    biosim_gpu_bench_report_print(&metrics, stdout);

exit:
    biosim_gpu_pipeline_free(&pipeline);
    biosim_gpu_runner_free(&runner);
    biosim_sim_free(&sim);
    biosim_params_free(&p);

    if (returncode != BIOSIM_OK) {
        BIOSIM_ERRORF("biosim-gpu-bench exiting with error (%s)", biosim_strerror(returncode));
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
