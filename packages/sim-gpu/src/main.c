#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "biosim/cfgparse/barriers.h"
#include "biosim/cfgparse/challenges.h"
#include "biosim/core/census.h"
#include "biosim/core/log.h"
#include "biosim/core/params.h"
#include "biosim/core/sim.h"
#include "biosim/core/status.h"
#include "biosim/sim-gpu/pipeline.h"
#include "biosim/sim-gpu/runner.h"

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
    {"verbose",                   NULL,         {.i = 0},         PARAM_COUNT,  false, false, "verbose",        "v"},
    {"population",                "simulation", {.i = 3000},      PARAM_INT,    false, true,  "pop",            "p"},
    {"grid-size-x",               "simulation", {.i = 128},       PARAM_INT,    false, true,  "grid-size-x",    "x"},
    {"grid-size-y",               "simulation", {.i = 128},       PARAM_INT,    false, true,  "grid-size-y",    "y"},
    {"steps-per-gen",             "simulation", {.i = 300},       PARAM_INT,    false, true,  "steps-per-gen",  NULL},
    {"max-generations",           "simulation", {.i = 1000},      PARAM_INT,    false, true,  "max-gen",        NULL},
    {"max-genome-len",            "genome",     {.i = 24},        PARAM_INT,    false, true,  "max-genome-len", NULL},
    {"max-neurons",               "genome",     {.i = 5},         PARAM_INT,    false, true,  "max-neurons",    NULL},
    {"point-mutation-rate",       "genome",     {.f = 0.001},     PARAM_FLOAT,  false, true,  "point-mut-rate", NULL},
    {"sexual-reproduction",       "genome",     {.b = false},     PARAM_BOOL,   false, true,  NULL,             NULL},
    {"choose-parents-by-fitness", "genome",     {.b = false},     PARAM_BOOL,   false, true,  NULL,             NULL},
    {"los-range",                 "sensors",    {.i = 16},        PARAM_INT,    false, true,  NULL,             NULL},
    {"sensor-radius",             "sensors",    {.i = 2},         PARAM_INT,    false, true,  NULL,             NULL},
    {"enable-kill",               "actions",    {.b = false},     PARAM_BOOL,   false, true,  "enable-kill",    NULL},
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
#define SIM_PARAMS_COUNT (sizeof(sim_params) / sizeof(sim_params[0]))

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
int main(int argc, char **argv) {
    /* alloc start here, free on exit label */
    biosim_params_t p;
    biosim_sim_t sim;
    biosim_gpu_runner_t runner;
    biosim_gpu_pipeline_t pipeline;
    biosim_barrier_spec_t *barriers = NULL;
    biosim_challenge_spec_t challenge;
    biosim_status_t returncode = BIOSIM_OK;

    memset(&sim, 0, sizeof(sim));
    memset(&runner, 0, sizeof(runner));
    memset(&pipeline, 0, sizeof(pipeline));
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

    uint32_t n_barriers = 0U;
    returncode = biosim_barrier_params_load(p.config_path, &barriers, &n_barriers);
    if (returncode != BIOSIM_OK) {
        goto exit;
    }

    returncode = biosim_challenge_spec_from_params(&p, &challenge);
    if (returncode != BIOSIM_OK) {
        goto exit;
    }

    returncode = biosim_sim_create(&sim, &p, &challenge, barriers, n_barriers);
    if (returncode != BIOSIM_OK) {
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

    returncode = biosim_gpu_pipeline_create(&sim, &runner, exec_dir, &pipeline);
    if (returncode != BIOSIM_OK) {
        goto exit;
    }

    /* ── main generation loop ────────────────────────────────────────────── */

    biosim_census_print_header(stdout);

    while (sim.gen < sim.max_generations && !g_halt_requested) {
        while (sim.step < sim.steps_per_gen) {
            returncode = biosim_gpu_pipeline_step(&pipeline, &sim);
            if (returncode != BIOSIM_OK) {
                goto exit;
            }
            sim.step++;
        }

        returncode = biosim_gpu_pipeline_sync_to_host(&pipeline, &sim);
        if (returncode != BIOSIM_OK) {
            goto exit;
        }

        biosim_census_t census;
        returncode = biosim_sim_next_generation(&sim, &census);
        if (returncode != BIOSIM_OK) {
            goto exit;
        }
        biosim_census_print(stdout, &census);

        returncode = biosim_gpu_pipeline_sync_from_host(&pipeline, &sim);
        if (returncode != BIOSIM_OK) {
            goto exit;
        }
    }

    if (g_halt_requested) {
        BIOSIM_WARNF("simulation halted by signal after generation %u", sim.gen);
    }

exit:
    biosim_gpu_pipeline_free(&pipeline);
    biosim_gpu_runner_free(&runner);
    biosim_sim_free(&sim);
    free(barriers);
    biosim_params_free(&p);

    if (returncode != BIOSIM_OK) {
        BIOSIM_ERRORF("biosim-gpu exiting with error (%s)", biosim_strerror(returncode));
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
