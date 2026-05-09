#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/opencl.h>
#endif

#include "biosim/core/io_catalogue.h"
#include "biosim/core/log.h"
#include "biosim/core/nnet.h"
#include "biosim/core/rng.h"
#include "biosim/core/sim.h"
#include "biosim/core/status.h"
#include "biosim/sim-gpu/registry.h"
#include "biosim/sim-gpu/runner.h"
#include "unity.h"

/* ── test fixture ───────────────────────────────────────────────────────── */

static biosim_sim_t g_sim;
static biosim_gpu_runner_t g_runner;
static cl_program g_program;
static cl_kernel g_kernel;
static int g_opencl_ok;

/* Per-agent GPU output buffers (host-side readback). */
static int16_t *g_gpu_desired_x;
static int16_t *g_gpu_desired_y;

/* Snapshots of mutable per-agent fields taken at fixture_setup time.
 * These are re-uploaded to the GPU at the start of each run_k1() call and
 * restored into g_sim for each run_host_step_agent() call, ensuring that
 * successive test functions see identical initial conditions. */
static uint64_t *g_init_rng_state;
static float *g_init_responsiveness;
static uint16_t *g_init_osc_period;
static uint8_t *g_init_long_probe_dist;
/* neuron_output and signal start at 0 (calloc in biosim_sim_create); no extra
 * snapshot needed — zeros are re-uploaded on each run. */

/* GPU buffers. */
static cl_mem g_buf_alive;
static cl_mem g_buf_loc_x;
static cl_mem g_buf_loc_y;
static cl_mem g_buf_osc_period;
static cl_mem g_buf_last_move_dir;
static cl_mem g_buf_responsiveness;
static cl_mem g_buf_long_probe_dist;
static cl_mem g_buf_conn_packed;
static cl_mem g_buf_conn_weight;
static cl_mem g_buf_conn_length;
static cl_mem g_buf_neuron_output;
static cl_mem g_buf_neuron_driven;
static cl_mem g_buf_neuron_count;
static cl_mem g_buf_signal;
static cl_mem g_buf_rng_state;
static cl_mem g_buf_desired_x;
static cl_mem g_buf_desired_y;

static void make_test_sim(biosim_sim_t *sim) {
    memset(sim, 0, sizeof(*sim));
    sim->population = 32U;
    sim->size_x = 16;
    sim->size_y = 16;
    sim->genome_max_len = 4U;
    sim->max_neurons = 2U;
    sim->long_probe_dist = 4U;
    sim->steps_per_gen = 100U;
    sim->step = 0U;
    sim->gen_rng = biosim_rng_seed(0U, 1U);
}

/* ── OpenCL availability probe ──────────────────────────────────────────── */

static int opencl_available(void) {
    cl_uint n = 0U;
    return (clGetPlatformIDs(0U, NULL, &n) == CL_SUCCESS && n > 0U) ? 1 : 0;
}

/* ── Unity setUp / tearDown ─────────────────────────────────────────────── */

void setUp(void) {
}
void tearDown(void) {
}

/* ── global fixture setup / teardown ────────────────────────────────────── */

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
static void fixture_setup(void) {
    biosim_log_init(&biosim_log_default_ctx);

    g_opencl_ok = opencl_available();
    if (!g_opencl_ok) {
        return;
    }

    make_test_sim(&g_sim);
    if (biosim_sim_create(&g_sim, NULL, 0U) != BIOSIM_OK) {
        g_opencl_ok = 0;
        return;
    }

    if (biosim_gpu_runner_create(0U, 0U, &g_runner) != BIOSIM_OK) {
        g_opencl_ok = 0;
        return;
    }

    biosim_gpu_kernel_sources_t sources;
    memset(&sources, 0, sizeof(sources));
    if (biosim_gpu_registry_get("k1_feedforward", NULL, &sources) != BIOSIM_OK) {
        g_opencl_ok = 0;
        return;
    }
    if (biosim_gpu_program_build(&g_runner, sources.sources, sources.count, &g_program) !=
        BIOSIM_OK) {
        biosim_gpu_kernel_sources_free(&sources);
        g_opencl_ok = 0;
        return;
    }
    biosim_gpu_kernel_sources_free(&sources);

    cl_int cl_err = CL_SUCCESS;
    g_kernel = clCreateKernel(g_program, "k_feedforward", &cl_err);
    if (cl_err != CL_SUCCESS || !g_kernel) {
        g_opencl_ok = 0;
        return;
    }

    uint32_t pop = g_sim.population;
    const biosim_agents_t *a = &g_sim.agents;

    /* Allocate host-side readback and snapshot buffers. */
    g_gpu_desired_x = malloc((size_t)pop * sizeof(int16_t));
    g_gpu_desired_y = malloc((size_t)pop * sizeof(int16_t));
    g_init_rng_state = malloc((size_t)pop * sizeof(uint64_t));
    g_init_responsiveness = malloc((size_t)pop * sizeof(float));
    g_init_osc_period = malloc((size_t)pop * sizeof(uint16_t));
    g_init_long_probe_dist = malloc((size_t)pop * sizeof(uint8_t));
    if (!g_gpu_desired_x || !g_gpu_desired_y || !g_init_rng_state || !g_init_responsiveness ||
        !g_init_osc_period || !g_init_long_probe_dist) {
        g_opencl_ok = 0;
        return;
    }

    /* Save initial mutable per-agent state. */
    memcpy(g_init_rng_state, a->rng_state, (size_t)pop * sizeof(uint64_t));
    memcpy(g_init_responsiveness, a->responsiveness, (size_t)pop * sizeof(float));
    memcpy(g_init_osc_period, a->osc_period, (size_t)pop * sizeof(uint16_t));
    memcpy(g_init_long_probe_dist, a->long_probe_dist, (size_t)pop * sizeof(uint8_t));

    const biosim_nnet_t *n = &g_sim.nnet;
    const uint16_t mc = n->max_conn;
    const uint8_t mn = n->max_neurons;

#define MKRO(var, ptr, esz, cnt)                                                                   \
    do {                                                                                           \
        (var) = clCreateBuffer(g_runner.context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,          \
                               (size_t)(cnt) * (esz), (void *)(ptr), &cl_err);                     \
        if (cl_err != CL_SUCCESS || !(var)) {                                                      \
            g_opencl_ok = 0;                                                                       \
            return;                                                                                \
        }                                                                                          \
    } while (0)

#define MKRW(var, ptr, esz, cnt)                                                                   \
    do {                                                                                           \
        (var) = clCreateBuffer(g_runner.context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,         \
                               (size_t)(cnt) * (esz), (void *)(ptr), &cl_err);                     \
        if (cl_err != CL_SUCCESS || !(var)) {                                                      \
            g_opencl_ok = 0;                                                                       \
            return;                                                                                \
        }                                                                                          \
    } while (0)

#define MKWO(var, esz, cnt)                                                                        \
    do {                                                                                           \
        (var) = clCreateBuffer(g_runner.context, CL_MEM_WRITE_ONLY, (size_t)(cnt) * (esz), NULL,   \
                               &cl_err);                                                           \
        if (cl_err != CL_SUCCESS || !(var)) {                                                      \
            g_opencl_ok = 0;                                                                       \
            return;                                                                                \
        }                                                                                          \
    } while (0)

    MKRO(g_buf_alive, a->alive, sizeof(uint8_t), pop);
    MKRO(g_buf_loc_x, a->loc_x, sizeof(int16_t), pop);
    MKRO(g_buf_loc_y, a->loc_y, sizeof(int16_t), pop);
    MKRW(g_buf_osc_period, a->osc_period, sizeof(uint16_t), pop);
    MKRO(g_buf_last_move_dir, a->last_move_dir, sizeof(uint8_t), pop);
    MKRW(g_buf_responsiveness, a->responsiveness, sizeof(float), pop);
    MKRW(g_buf_long_probe_dist, a->long_probe_dist, sizeof(uint8_t), pop);
    MKRO(g_buf_conn_packed, n->genome_conn, sizeof(uint16_t), (size_t)mc * pop);
    MKRO(g_buf_conn_weight, n->genome_wgt, sizeof(int16_t), (size_t)mc * pop);
    MKRO(g_buf_conn_length, n->conn_length, sizeof(uint16_t), pop);
    MKRW(g_buf_neuron_output, n->neuron_output, sizeof(float), (size_t)mn * pop);
    MKRO(g_buf_neuron_driven, n->neuron_driven, sizeof(uint8_t), (size_t)mn * pop);
    MKRO(g_buf_neuron_count, n->neuron_count, sizeof(uint8_t), pop);
    MKRW(g_buf_signal, g_sim.signal, sizeof(uint32_t), g_sim.signal_len);
    MKRW(g_buf_rng_state, a->rng_state, sizeof(uint64_t), pop);
    MKWO(g_buf_desired_x, sizeof(int16_t), pop);
    MKWO(g_buf_desired_y, sizeof(int16_t), pop);

#undef MKRO
#undef MKRW
#undef MKWO

    /* Set all kernel arguments (scalars stay fixed across runs). */
    cl_int size_x = (cl_int)g_sim.size_x;
    cl_int size_y = (cl_int)g_sim.size_y;
    cl_uint step = (cl_uint)g_sim.step;
    cl_uint steps_gen = (cl_uint)g_sim.steps_per_gen;
    cl_uint pop_arg = (cl_uint)g_sim.population;

    (void)clSetKernelArg(g_kernel, 0U, sizeof(cl_mem), (const void *)&g_buf_alive);
    (void)clSetKernelArg(g_kernel, 1U, sizeof(cl_mem), (const void *)&g_buf_loc_x);
    (void)clSetKernelArg(g_kernel, 2U, sizeof(cl_mem), (const void *)&g_buf_loc_y);
    (void)clSetKernelArg(g_kernel, 3U, sizeof(cl_mem), (const void *)&g_buf_osc_period);
    (void)clSetKernelArg(g_kernel, 4U, sizeof(cl_mem), (const void *)&g_buf_last_move_dir);
    (void)clSetKernelArg(g_kernel, 5U, sizeof(cl_mem), (const void *)&g_buf_responsiveness);
    (void)clSetKernelArg(g_kernel, 6U, sizeof(cl_mem), (const void *)&g_buf_long_probe_dist);
    (void)clSetKernelArg(g_kernel, 7U, sizeof(cl_mem), (const void *)&g_buf_conn_packed);
    (void)clSetKernelArg(g_kernel, 8U, sizeof(cl_mem), (const void *)&g_buf_conn_weight);
    (void)clSetKernelArg(g_kernel, 9U, sizeof(cl_mem), (const void *)&g_buf_conn_length);
    (void)clSetKernelArg(g_kernel, 10U, sizeof(cl_mem), (const void *)&g_buf_neuron_output);
    (void)clSetKernelArg(g_kernel, 11U, sizeof(cl_mem), (const void *)&g_buf_neuron_driven);
    (void)clSetKernelArg(g_kernel, 12U, sizeof(cl_mem), (const void *)&g_buf_neuron_count);
    (void)clSetKernelArg(g_kernel, 13U, sizeof(cl_mem), (const void *)&g_buf_signal);
    (void)clSetKernelArg(g_kernel, 14U, sizeof(cl_int), &size_x);
    (void)clSetKernelArg(g_kernel, 15U, sizeof(cl_int), &size_y);
    (void)clSetKernelArg(g_kernel, 16U, sizeof(cl_uint), &step);
    (void)clSetKernelArg(g_kernel, 17U, sizeof(cl_uint), &steps_gen);
    (void)clSetKernelArg(g_kernel, 18U, sizeof(cl_uint), &pop_arg);
    (void)clSetKernelArg(g_kernel, 19U, sizeof(cl_mem), (const void *)&g_buf_rng_state);
    (void)clSetKernelArg(g_kernel, 20U, sizeof(cl_mem), (const void *)&g_buf_desired_x);
    (void)clSetKernelArg(g_kernel, 21U, sizeof(cl_mem), (const void *)&g_buf_desired_y);
}

static void fixture_teardown(void) {
    free(g_gpu_desired_x);
    free(g_gpu_desired_y);
    free(g_init_rng_state);
    free(g_init_responsiveness);
    free(g_init_osc_period);
    free(g_init_long_probe_dist);
    g_gpu_desired_x = NULL;
    g_gpu_desired_y = NULL;
    g_init_rng_state = NULL;
    g_init_responsiveness = NULL;
    g_init_osc_period = NULL;
    g_init_long_probe_dist = NULL;

#define REL(v)                                                                                     \
    do {                                                                                           \
        if (v) {                                                                                   \
            clReleaseMemObject(v);                                                                 \
            (v) = NULL;                                                                            \
        }                                                                                          \
    } while (0)
    REL(g_buf_desired_y);
    REL(g_buf_desired_x);
    REL(g_buf_rng_state);
    REL(g_buf_signal);
    REL(g_buf_neuron_count);
    REL(g_buf_neuron_driven);
    REL(g_buf_neuron_output);
    REL(g_buf_conn_length);
    REL(g_buf_conn_weight);
    REL(g_buf_conn_packed);
    REL(g_buf_long_probe_dist);
    REL(g_buf_responsiveness);
    REL(g_buf_last_move_dir);
    REL(g_buf_osc_period);
    REL(g_buf_loc_y);
    REL(g_buf_loc_x);
    REL(g_buf_alive);
#undef REL

    if (g_kernel) {
        clReleaseKernel(g_kernel);
        g_kernel = NULL;
    }
    if (g_program) {
        clReleaseProgram(g_program);
        g_program = NULL;
    }
    biosim_gpu_runner_free(&g_runner);
    biosim_sim_free(&g_sim);
}

/* ── helper: restore initial GPU state and dispatch K1 ─────────────────── */

/* Re-upload all mutable GPU buffers to their initial state, then dispatch K1
 * and read back desired_x/desired_y. Returns 1 on success. */
static int run_k1(void) {
    cl_command_queue q = g_runner.queue;
    uint32_t pop = g_sim.population;
    const biosim_nnet_t *n = &g_sim.nnet;

    /* Restore mutable per-agent fields to initial snapshot. */
    if (clEnqueueWriteBuffer(q, g_buf_rng_state, CL_FALSE, 0U, (size_t)pop * sizeof(uint64_t),
                             g_init_rng_state, 0U, NULL, NULL) != CL_SUCCESS) {
        return 0;
    }
    if (clEnqueueWriteBuffer(q, g_buf_responsiveness, CL_FALSE, 0U, (size_t)pop * sizeof(float),
                             g_init_responsiveness, 0U, NULL, NULL) != CL_SUCCESS) {
        return 0;
    }
    if (clEnqueueWriteBuffer(q, g_buf_osc_period, CL_FALSE, 0U, (size_t)pop * sizeof(uint16_t),
                             g_init_osc_period, 0U, NULL, NULL) != CL_SUCCESS) {
        return 0;
    }
    if (clEnqueueWriteBuffer(q, g_buf_long_probe_dist, CL_FALSE, 0U, (size_t)pop * sizeof(uint8_t),
                             g_init_long_probe_dist, 0U, NULL, NULL) != CL_SUCCESS) {
        return 0;
    }

    /* neuron_output starts at 0 from biosim_sim_create (calloc). */
    size_t neuron_bytes = (size_t)n->max_neurons * (size_t)pop * sizeof(float);
    if (clEnqueueWriteBuffer(q, g_buf_neuron_output, CL_FALSE, 0U, neuron_bytes, n->neuron_output,
                             0U, NULL, NULL) != CL_SUCCESS) {
        return 0;
    }

    /* signal starts at 0 (calloc in biosim_sim_create). */
    if (clEnqueueWriteBuffer(q, g_buf_signal, CL_TRUE, 0U, g_sim.signal_len * sizeof(uint32_t),
                             g_sim.signal, 0U, NULL, NULL) != CL_SUCCESS) {
        return 0;
    }

    size_t global_size = (size_t)pop;
    if (clEnqueueNDRangeKernel(q, g_kernel, 1U, NULL, &global_size, NULL, 0U, NULL, NULL) !=
        CL_SUCCESS) {
        return 0;
    }

    if (clEnqueueReadBuffer(q, g_buf_desired_x, CL_FALSE, 0U, (size_t)pop * sizeof(int16_t),
                            g_gpu_desired_x, 0U, NULL, NULL) != CL_SUCCESS) {
        return 0;
    }
    if (clEnqueueReadBuffer(q, g_buf_desired_y, CL_TRUE, 0U, (size_t)pop * sizeof(int16_t),
                            g_gpu_desired_y, 0U, NULL, NULL) != CL_SUCCESS) {
        return 0;
    }
    return 1;
}

/* Run the host reference for one agent: sensor eval → feedforward → actions →
 * movement finalize. Does NOT update the grid. Restores all mutable agent
 * fields from the initial snapshot first so multiple calls give reproducible
 * results. */
static void run_host_step_agent(uint32_t idx) {
    biosim_agents_t *a = &g_sim.agents;
    biosim_nnet_t *n = &g_sim.nnet;
    const uint32_t pop = g_sim.population;

    /* Restore all mutable per-agent fields to initial snapshot. */
    a->rng_state[idx] = g_init_rng_state[idx];
    a->responsiveness[idx] = g_init_responsiveness[idx];
    a->osc_period[idx] = g_init_osc_period[idx];
    a->long_probe_dist[idx] = g_init_long_probe_dist[idx];
    for (uint8_t k = 0U; k < n->max_neurons; k++) {
        n->neuron_output[(size_t)k * pop + idx] = 0.0F;
    }

    float sensor_vals[BIOSIM_NUM_SENSORS];
    float action_vals[BIOSIM_NUM_ACTIONS];

    for (uint32_t s = 0U; s < BIOSIM_NUM_SENSORS; s++) {
        sensor_vals[s] = biosim_sensor_eval((biosim_sensor_t)s, idx, &g_sim);
    }

    memset(action_vals, 0, sizeof(action_vals));
    biosim_nnet_feedforward(n, idx, sensor_vals, BIOSIM_NUM_SENSORS, action_vals,
                            BIOSIM_NUM_ACTIONS);

    a->dx_sum[idx] = 0.0F;
    a->dy_sum[idx] = 0.0F;

    for (uint32_t act = 0U; act < BIOSIM_NUM_ACTIONS; act++) {
        biosim_action_apply((biosim_action_t)act, action_vals[act], idx, &g_sim);
    }

    biosim_action_finalize_movement(idx, &g_sim);
}

/* ── tests ──────────────────────────────────────────────────────────────── */

/* Verify that K1 dispatches successfully and all alive agents receive a valid
 * (in-grid-bounds) desired position. */
void test_k1_compiles_and_runs(void) {
    if (!g_opencl_ok) {
        TEST_IGNORE_MESSAGE("OpenCL not available");
    }

    TEST_ASSERT_NOT_NULL(g_kernel);

    TEST_ASSERT_TRUE_MESSAGE(run_k1(), "K1 kernel dispatch failed");

    uint32_t pop = g_sim.population;
    for (uint32_t i = 0U; i < pop; i++) {
        if (!g_sim.agents.alive[i]) {
            continue;
        }
        TEST_ASSERT_TRUE(g_gpu_desired_x[i] >= 0);
        TEST_ASSERT_TRUE(g_gpu_desired_x[i] < (int16_t)g_sim.size_x);
        TEST_ASSERT_TRUE(g_gpu_desired_y[i] >= 0);
        TEST_ASSERT_TRUE(g_gpu_desired_y[i] < (int16_t)g_sim.size_y);
    }
}

/* Compare GPU desired positions against the host reference for every alive
 * agent. Both sides start from the same initial state (rng_state, neuron
 * outputs, responsiveness, osc_period, long_probe_dist, signal=0), so they
 * must produce identical results. */
void test_k1_matches_host_reference(void) {
    if (!g_opencl_ok) {
        TEST_IGNORE_MESSAGE("OpenCL not available");
    }

    TEST_ASSERT_TRUE_MESSAGE(run_k1(), "K1 kernel dispatch failed");

    uint32_t pop = g_sim.population;

    /* Reset g_sim.signal to 0 before running host reference so that
     * SIGNAL0 sensor reads 0.0F for all agents (same as GPU sees from the
     * initial zero signal buffer). */
    memset(g_sim.signal, 0, g_sim.signal_len * sizeof(uint32_t));

    uint32_t mismatches = 0U;
    for (uint32_t i = 0U; i < pop; i++) {
        if (!g_sim.agents.alive[i]) {
            continue;
        }
        run_host_step_agent(i);

        if (g_gpu_desired_x[i] != g_sim.agents.desired_x[i] ||
            g_gpu_desired_y[i] != g_sim.agents.desired_y[i]) {
            mismatches++;
        }
    }

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0U, mismatches,
                                     "desired_x/desired_y mismatch(es) between GPU and host");
}

/* ── main ───────────────────────────────────────────────────────────────── */

int main(void) {
    UNITY_BEGIN();
    fixture_setup();
    RUN_TEST(test_k1_compiles_and_runs);
    RUN_TEST(test_k1_matches_host_reference);
    fixture_teardown();
    return UNITY_END();
}
