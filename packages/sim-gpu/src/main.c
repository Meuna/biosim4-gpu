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

/* ── buffer layout helpers ──────────────────────────────────────────────── */

typedef struct {
    /* Per-agent fixed fields */
    cl_mem alive;
    cl_mem loc_x; /* RW: K1 reads, K2 writes */
    cl_mem loc_y; /* RW: K1 reads, K2 writes */
    cl_mem osc_period;
    cl_mem last_move_dir; /* RW: K1 reads, K2 writes */
    cl_mem responsiveness;
    cl_mem long_probe_dist;
    /* Neural network (transposed SoA) */
    cl_mem conn_packed;
    cl_mem conn_weight;
    cl_mem conn_length;
    cl_mem neuron_output;
    cl_mem neuron_driven;
    cl_mem neuron_count;
    /* Signal */
    cl_mem signal;
    /* K1 output / K2 input */
    cl_mem rng_state;
    cl_mem desired_x;   /* RW: K1 writes, K3 reads */
    cl_mem desired_y;   /* RW: K1 writes, K3 reads */
    cl_mem kill_marker; /* RW: K1 writes, K2 reads */
    /* Grid: K2/K3 in/out, uint32_t per cell (required for OpenCL atomics) */
    cl_mem grid;
} kernel_buffers_t;

static void kernel_buffers_release(kernel_buffers_t *b) {
    CL_SAFE_RELEASE(clReleaseMemObject, b->grid);
    CL_SAFE_RELEASE(clReleaseMemObject, b->kill_marker);
    CL_SAFE_RELEASE(clReleaseMemObject, b->desired_y);
    CL_SAFE_RELEASE(clReleaseMemObject, b->desired_x);
    CL_SAFE_RELEASE(clReleaseMemObject, b->rng_state);
    CL_SAFE_RELEASE(clReleaseMemObject, b->signal);
    CL_SAFE_RELEASE(clReleaseMemObject, b->neuron_count);
    CL_SAFE_RELEASE(clReleaseMemObject, b->neuron_driven);
    CL_SAFE_RELEASE(clReleaseMemObject, b->neuron_output);
    CL_SAFE_RELEASE(clReleaseMemObject, b->conn_length);
    CL_SAFE_RELEASE(clReleaseMemObject, b->conn_weight);
    CL_SAFE_RELEASE(clReleaseMemObject, b->conn_packed);
    CL_SAFE_RELEASE(clReleaseMemObject, b->long_probe_dist);
    CL_SAFE_RELEASE(clReleaseMemObject, b->responsiveness);
    CL_SAFE_RELEASE(clReleaseMemObject, b->last_move_dir);
    CL_SAFE_RELEASE(clReleaseMemObject, b->osc_period);
    CL_SAFE_RELEASE(clReleaseMemObject, b->loc_y);
    CL_SAFE_RELEASE(clReleaseMemObject, b->loc_x);
    CL_SAFE_RELEASE(clReleaseMemObject, b->alive);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
static biosim_status_t kernel_buffers_create(const biosim_sim_t *sim, cl_context ctx,
                                             kernel_buffers_t *out) {
    memset(out, 0, sizeof(*out));

    const biosim_agents_t *a = &sim->agents;
    const biosim_nnet_t *n = &sim->nnet;
    const uint32_t pop = sim->population;
    const uint16_t max_conn = n->max_conn;
    const uint8_t max_neurons = n->max_neurons;

    biosim_status_t returncode = BIOSIM_OK;
    cl_int cl_err = CL_SUCCESS;

#define MKBUF_RO(field, ptr, elem_size, count)                                                     \
    CL_ASSIGN_OR_GOTO_EXIT(out->field,                                                             \
                           clCreateBuffer(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,            \
                                          (size_t)(count) * (elem_size), (void *)(ptr), &cl_err))

#define MKBUF_RW(field, ptr, elem_size, count)                                                     \
    CL_ASSIGN_OR_GOTO_EXIT(out->field,                                                             \
                           clCreateBuffer(ctx, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,           \
                                          (size_t)(count) * (elem_size), (void *)(ptr), &cl_err))

#define MKBUF_WO(field, elem_size, count)                                                          \
    CL_ASSIGN_OR_GOTO_EXIT(out->field,                                                             \
                           clCreateBuffer(ctx, CL_MEM_WRITE_ONLY, (size_t)(count) * (elem_size),   \
                                          NULL, &cl_err))

    MKBUF_RW(alive, a->alive, sizeof(uint8_t), pop);
    MKBUF_RW(loc_x, a->loc_x, sizeof(int16_t), pop);
    MKBUF_RW(loc_y, a->loc_y, sizeof(int16_t), pop);
    MKBUF_RW(osc_period, a->osc_period, sizeof(uint16_t), pop);
    MKBUF_RW(last_move_dir, a->last_move_dir, sizeof(uint8_t), pop);
    MKBUF_RW(responsiveness, a->responsiveness, sizeof(float), pop);
    MKBUF_RW(long_probe_dist, a->long_probe_dist, sizeof(uint8_t), pop);
    MKBUF_RO(conn_packed, n->genome_conn, sizeof(uint16_t), (size_t)max_conn * pop);
    MKBUF_RO(conn_weight, n->genome_wgt, sizeof(int16_t), (size_t)max_conn * pop);
    MKBUF_RO(conn_length, n->conn_length, sizeof(uint16_t), pop);
    MKBUF_RW(neuron_output, n->neuron_output, sizeof(float), (size_t)max_neurons * pop);
    MKBUF_RO(neuron_driven, n->neuron_driven, sizeof(uint8_t), (size_t)max_neurons * pop);
    MKBUF_RO(neuron_count, n->neuron_count, sizeof(uint8_t), pop);
    MKBUF_RW(signal, sim->signal, sizeof(uint32_t), sim->signal_len);
    MKBUF_RW(rng_state, a->rng_state, sizeof(uint64_t), pop);
    MKBUF_RW(desired_x, a->desired_x, sizeof(int16_t), pop);
    MKBUF_RW(desired_y, a->desired_y, sizeof(int16_t), pop);
    MKBUF_RW(kill_marker, a->kill_marker, sizeof(uint8_t), pop);

    /* Grid: convert uint16_t cells → uint32_t for OpenCL atomic operations. */
    {
        size_t grid_cells = (size_t)sim->size_x * (size_t)sim->size_y;
        uint32_t *tmp_grid = malloc(grid_cells * sizeof(uint32_t));
        if (!tmp_grid) {
            returncode = BIOSIM_ERR_NOMEM;
            goto exit;
        }
        for (size_t gi = 0U; gi < grid_cells; gi++) {
            tmp_grid[gi] = (uint32_t)sim->grid.cells[gi];
        }
        MKBUF_RW(grid, tmp_grid, sizeof(uint32_t), grid_cells);
        free(tmp_grid);
    }

#undef MKBUF_RO
#undef MKBUF_RW
#undef MKBUF_WO

exit:
    if (returncode != BIOSIM_OK) {
        kernel_buffers_release(out);
    }
    return returncode;
}

static void k1_set_args(cl_kernel kernel, const kernel_buffers_t *b, const biosim_sim_t *sim) {
    cl_int size_x = (cl_int)sim->size_x;
    cl_int size_y = (cl_int)sim->size_y;
    cl_uint step = (cl_uint)sim->step;
    cl_uint steps_gen = (cl_uint)sim->steps_per_gen;
    cl_uint pop = (cl_uint)sim->population;
    cl_int enable_kill_val = (cl_int)(sim->enable_kill ? 1 : 0);

    (void)clSetKernelArg(kernel, 0U, sizeof(cl_mem), (const void *)&b->alive);
    (void)clSetKernelArg(kernel, 1U, sizeof(cl_mem), (const void *)&b->loc_x);
    (void)clSetKernelArg(kernel, 2U, sizeof(cl_mem), (const void *)&b->loc_y);
    (void)clSetKernelArg(kernel, 3U, sizeof(cl_mem), (const void *)&b->osc_period);
    (void)clSetKernelArg(kernel, 4U, sizeof(cl_mem), (const void *)&b->last_move_dir);
    (void)clSetKernelArg(kernel, 5U, sizeof(cl_mem), (const void *)&b->responsiveness);
    (void)clSetKernelArg(kernel, 6U, sizeof(cl_mem), (const void *)&b->long_probe_dist);
    (void)clSetKernelArg(kernel, 7U, sizeof(cl_mem), (const void *)&b->conn_packed);
    (void)clSetKernelArg(kernel, 8U, sizeof(cl_mem), (const void *)&b->conn_weight);
    (void)clSetKernelArg(kernel, 9U, sizeof(cl_mem), (const void *)&b->conn_length);
    (void)clSetKernelArg(kernel, 10U, sizeof(cl_mem), (const void *)&b->neuron_output);
    (void)clSetKernelArg(kernel, 11U, sizeof(cl_mem), (const void *)&b->neuron_driven);
    (void)clSetKernelArg(kernel, 12U, sizeof(cl_mem), (const void *)&b->neuron_count);
    (void)clSetKernelArg(kernel, 13U, sizeof(cl_mem), (const void *)&b->signal);
    (void)clSetKernelArg(kernel, 14U, sizeof(cl_int), (const void *)&size_x);
    (void)clSetKernelArg(kernel, 15U, sizeof(cl_int), (const void *)&size_y);
    (void)clSetKernelArg(kernel, 16U, sizeof(cl_uint), (const void *)&step);
    (void)clSetKernelArg(kernel, 17U, sizeof(cl_uint), (const void *)&steps_gen);
    (void)clSetKernelArg(kernel, 18U, sizeof(cl_uint), (const void *)&pop);
    (void)clSetKernelArg(kernel, 19U, sizeof(cl_mem), (const void *)&b->rng_state);
    (void)clSetKernelArg(kernel, 20U, sizeof(cl_mem), (const void *)&b->desired_x);
    (void)clSetKernelArg(kernel, 21U, sizeof(cl_mem), (const void *)&b->desired_y);
    (void)clSetKernelArg(kernel, 22U, sizeof(cl_mem), (const void *)&b->grid);
    (void)clSetKernelArg(kernel, 23U, sizeof(cl_int), (const void *)&enable_kill_val);
    (void)clSetKernelArg(kernel, 24U, sizeof(cl_mem), (const void *)&b->kill_marker);
}

static void k2_kill_set_args(cl_kernel kernel, const kernel_buffers_t *b, const biosim_sim_t *sim) {
    cl_int size_x = (cl_int)sim->size_x;
    cl_uint pop = (cl_uint)sim->population;

    (void)clSetKernelArg(kernel, 0U, sizeof(cl_mem), (const void *)&b->kill_marker);
    (void)clSetKernelArg(kernel, 1U, sizeof(cl_mem), (const void *)&b->loc_x);
    (void)clSetKernelArg(kernel, 2U, sizeof(cl_mem), (const void *)&b->loc_y);
    (void)clSetKernelArg(kernel, 3U, sizeof(cl_mem), (const void *)&b->grid);
    (void)clSetKernelArg(kernel, 4U, sizeof(cl_int), (const void *)&size_x);
    (void)clSetKernelArg(kernel, 5U, sizeof(cl_uint), (const void *)&pop);
}

static void k3_set_args(cl_kernel kernel, const kernel_buffers_t *b, const biosim_sim_t *sim) {
    cl_int size_x = (cl_int)sim->size_x;
    cl_int size_y = (cl_int)sim->size_y;
    cl_uint pop = (cl_uint)sim->population;

    (void)clSetKernelArg(kernel, 0U, sizeof(cl_mem), (const void *)&b->alive);
    (void)clSetKernelArg(kernel, 1U, sizeof(cl_mem), (const void *)&b->desired_x);
    (void)clSetKernelArg(kernel, 2U, sizeof(cl_mem), (const void *)&b->desired_y);
    (void)clSetKernelArg(kernel, 3U, sizeof(cl_mem), (const void *)&b->loc_x);
    (void)clSetKernelArg(kernel, 4U, sizeof(cl_mem), (const void *)&b->loc_y);
    (void)clSetKernelArg(kernel, 5U, sizeof(cl_mem), (const void *)&b->last_move_dir);
    (void)clSetKernelArg(kernel, 6U, sizeof(cl_mem), (const void *)&b->grid);
    (void)clSetKernelArg(kernel, 7U, sizeof(cl_int), (const void *)&size_x);
    (void)clSetKernelArg(kernel, 8U, sizeof(cl_int), (const void *)&size_y);
    (void)clSetKernelArg(kernel, 9U, sizeof(cl_uint), (const void *)&pop);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
int main(int argc, char **argv) {
    /* alloc start here, free on exit label */
    biosim_params_t p;
    biosim_sim_t sim;
    biosim_gpu_runner_t runner;
    biosim_gpu_kernel_sources_t k1_sources;
    biosim_gpu_kernel_sources_t k2_sources;
    biosim_gpu_kernel_sources_t k3_sources;
    kernel_buffers_t bufs;
    cl_program k1_program = NULL;
    cl_kernel k1_kernel = NULL;
    cl_program k2_program = NULL;
    cl_kernel k2_kernel = NULL;
    cl_program k3_program = NULL;
    cl_kernel k3_kernel = NULL;
    int16_t *host_desired_x = NULL;
    int16_t *host_loc_x = NULL;
    biosim_status_t returncode = BIOSIM_OK;

    memset(&sim, 0, sizeof(sim));
    memset(&runner, 0, sizeof(runner));
    memset(&k1_sources, 0, sizeof(k1_sources));
    memset(&k2_sources, 0, sizeof(k2_sources));
    memset(&k3_sources, 0, sizeof(k3_sources));
    memset(&bufs, 0, sizeof(bufs));
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

    returncode = kernel_buffers_create(&sim, runner.context, &bufs);
    if (returncode != BIOSIM_OK) {
        goto exit;
    }

    /* ── K1: feedforward ─────────────────────────────────────────────────── */

    returncode = biosim_gpu_registry_get("k1_feedforward", exec_dir, &k1_sources);
    if (returncode != BIOSIM_OK) {
        goto exit;
    }

    returncode =
        biosim_gpu_program_build(&runner, k1_sources.sources, k1_sources.count, &k1_program);
    if (returncode != BIOSIM_OK) {
        goto exit;
    }

    cl_int cl_err = CL_SUCCESS;
    CL_ASSIGN_OR_GOTO_EXIT(k1_kernel, clCreateKernel(k1_program, "k_feedforward", &cl_err));

    k1_set_args(k1_kernel, &bufs, &sim);

    size_t global_size = (size_t)sim.population;
    CL_GOTO_EXIT_ON_ERROR(clEnqueueNDRangeKernel(runner.queue, k1_kernel, 1U, NULL, &global_size,
                                                 NULL, 0U, NULL, NULL));

    host_desired_x = malloc((size_t)sim.population * sizeof(int16_t));
    if (!host_desired_x) {
        returncode = BIOSIM_ERR_NOMEM;
        goto exit;
    }

    CL_GOTO_EXIT_ON_ERROR(clEnqueueReadBuffer(runner.queue, bufs.desired_x, CL_TRUE, 0U,
                                              (size_t)sim.population * sizeof(int16_t),
                                              host_desired_x, 0U, NULL, NULL));

    printf("biosim-gpu: K1 step complete — agent 0 desired_x=%d (delta=%d)\n",
           (int)host_desired_x[0], (int)(host_desired_x[0] - sim.agents.loc_x[0]));

    /* ── K2: kill_marked ─────────────────────────────────────────────────── */

    returncode = biosim_gpu_registry_get("k2_kill_marked", exec_dir, &k2_sources);
    if (returncode != BIOSIM_OK) {
        goto exit;
    }

    returncode =
        biosim_gpu_program_build(&runner, k2_sources.sources, k2_sources.count, &k2_program);
    if (returncode != BIOSIM_OK) {
        goto exit;
    }

    CL_ASSIGN_OR_GOTO_EXIT(k2_kernel, clCreateKernel(k2_program, "k_kill_marked", &cl_err));

    k2_kill_set_args(k2_kernel, &bufs, &sim);

    CL_GOTO_EXIT_ON_ERROR(clEnqueueNDRangeKernel(runner.queue, k2_kernel, 1U, NULL, &global_size,
                                                 NULL, 0U, NULL, NULL));

    /* ── K3: movement resolution ─────────────────────────────────────────── */

    returncode = biosim_gpu_registry_get("k3_movement_resolution", exec_dir, &k3_sources);
    if (returncode != BIOSIM_OK) {
        goto exit;
    }

    returncode =
        biosim_gpu_program_build(&runner, k3_sources.sources, k3_sources.count, &k3_program);
    if (returncode != BIOSIM_OK) {
        goto exit;
    }

    CL_ASSIGN_OR_GOTO_EXIT(k3_kernel, clCreateKernel(k3_program, "k_movement_resolution", &cl_err));

    k3_set_args(k3_kernel, &bufs, &sim);

    CL_GOTO_EXIT_ON_ERROR(clEnqueueNDRangeKernel(runner.queue, k3_kernel, 1U, NULL, &global_size,
                                                 NULL, 0U, NULL, NULL));

    host_loc_x = malloc((size_t)sim.population * sizeof(int16_t));
    if (!host_loc_x) {
        returncode = BIOSIM_ERR_NOMEM;
        goto exit;
    }

    CL_GOTO_EXIT_ON_ERROR(clEnqueueReadBuffer(runner.queue, bufs.loc_x, CL_TRUE, 0U,
                                              (size_t)sim.population * sizeof(int16_t), host_loc_x,
                                              0U, NULL, NULL));

    printf("biosim-gpu: K3 step complete — agent 0 loc_x=%d\n", (int)host_loc_x[0]);

exit:
    free(host_loc_x);
    free(host_desired_x);
    kernel_buffers_release(&bufs);
    CL_SAFE_RELEASE(clReleaseKernel, k1_kernel);
    CL_SAFE_RELEASE(clReleaseProgram, k1_program);
    biosim_gpu_kernel_sources_free(&k1_sources);
    CL_SAFE_RELEASE(clReleaseKernel, k2_kernel);
    CL_SAFE_RELEASE(clReleaseProgram, k2_program);
    biosim_gpu_kernel_sources_free(&k2_sources);
    CL_SAFE_RELEASE(clReleaseKernel, k3_kernel);
    CL_SAFE_RELEASE(clReleaseProgram, k3_program);
    biosim_gpu_kernel_sources_free(&k3_sources);
    biosim_gpu_runner_free(&runner);
    biosim_sim_free(&sim);
    biosim_params_free(&p);

    if (returncode != BIOSIM_OK) {
        BIOSIM_ERRORF("biosim-gpu exiting with error (%s)", biosim_strerror(returncode));
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
