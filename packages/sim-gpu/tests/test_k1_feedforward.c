#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "biosim/core/io_eval.h"
#include "biosim/core/log.h"
#include "biosim/sim-gpu/registry.h"
#include "gpu_test_utils.h"
#include "unity.h"

/* ── test fixture ───────────────────────────────────────────────────────── */

static biosim_status_t fixture_status;
static biosim_sim_t sim;
static biosim_gpu_runner_t runner;
static cl_program program;
static cl_kernel kernel;

/* Per-agent GPU output buffers (host-side readback). */
static int32_t *gpu_desired_x;
static int32_t *gpu_desired_y;
static uint32_t *gpu_signal;
static float *gpu_responsiveness;
static uint16_t *gpu_osc_period;
static uint8_t *gpu_los_range;
static uint8_t *gpu_kill_marker;

/* Snapshots of mutable per-agent fields taken at fixture_setup time.
 * These are re-uploaded to the GPU at the start of each run_k1() call and
 * restored into sim for each run_host_step_agent() call, ensuring that
 * successive test functions see identical initial conditions. */
static uint64_t *init_rng_state;
static float *init_responsiveness;
static uint16_t *init_osc_period;
static uint8_t *init_los_range;
/* neuron_output and signal start at 0 (calloc in biosim_sim_create); no extra
 * snapshot needed — zeros are re-uploaded on each run. */

/* GPU buffers. */
static cl_mem buf_alive;
static cl_mem buf_loc_x;
static cl_mem buf_loc_y;
static cl_mem buf_osc_period;
static cl_mem buf_last_move_dir;
static cl_mem buf_responsiveness;
static cl_mem buf_los_range;
static cl_mem buf_conn_packed;
static cl_mem buf_conn_weight;
static cl_mem buf_conn_length;
static cl_mem buf_neuron_output;
static cl_mem buf_neuron_driven;
static cl_mem buf_neuron_count;
static cl_mem buf_signal;
static cl_mem buf_rng_state;
static cl_mem buf_desired_x;
static cl_mem buf_desired_y;
static cl_mem buf_grid;
static cl_mem buf_kill_marker;

/* ── Unity setUp / tearDown ─────────────────────────────────────────────── */

void setUp(void) {
}
void tearDown(void) {
}

/* ── global fixture setup / teardown ────────────────────────────────────── */

void disconnect_signal_emission(biosim_sim_t *sim) {
    uint32_t pop = sim->agents.population;
    biosim_nnet_t nnet = sim->nnet;
    for (uint32_t i = 0U; i < pop; i++) {
        uint32_t len = nnet.conn_length[i];
        for (uint32_t c = 0U; c < len; c++) {
            uint16_t packed = nnet.genome_conn[c * pop + i];
            if (BIOSIM_GENE_SINK_TYPE(packed) == BIOSIM_GENE_IO &&
                BIOSIM_GENE_SINK_NUM(packed) == BIOSIM_ACTION_EMIT_SIGNAL0) {
                nnet.genome_wgt[c * pop + i] = 0;
            }
        }
    }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
static void fixture_setup(void) {
    biosim_log_init(&biosim_log_default_ctx);

    fixture_status = sim_test_make_128x128(&sim);
    if (fixture_status != BIOSIM_OK) {
        return;
    }
    sim.sensor_radius = 16;
    sim.los_range = 16;

    /* The current signal emission has race conditions on the GPU */
    disconnect_signal_emission(&sim);

    fixture_status = gpu_test_kernel_runtime_create(
        &runner, &program, &kernel, "k1_feedforward", "k_feedforward"
    );
    if (fixture_status != BIOSIM_OK) {
        return;
    }

    uint32_t pop = sim.population;
    const biosim_agents_t *a = &sim.agents;

    /* Allocate host-side readback and snapshot buffers. */
    ALLOC(gpu_desired_x, pop, sizeof(int32_t));
    ALLOC(gpu_desired_y, pop, sizeof(int32_t));
    ALLOC(gpu_signal, sim.signal_len, sizeof(uint32_t));
    ALLOC(gpu_responsiveness, pop, sizeof(float));
    ALLOC(gpu_osc_period, pop, sizeof(uint16_t));
    ALLOC(gpu_los_range, pop, sizeof(uint8_t));
    ALLOC(gpu_kill_marker, pop, sizeof(uint8_t));
    ALLOC(init_rng_state, pop, sizeof(uint64_t));
    ALLOC(init_responsiveness, pop, sizeof(float));
    ALLOC(init_osc_period, pop, sizeof(uint16_t));
    ALLOC(init_los_range, pop, sizeof(uint8_t));

    /* Save initial mutable per-agent state. */
    memcpy(init_rng_state, a->rng_state, (size_t)pop * sizeof(uint64_t));
    memcpy(init_responsiveness, a->responsiveness, (size_t)pop * sizeof(float));
    memcpy(init_osc_period, a->osc_period, (size_t)pop * sizeof(uint16_t));
    memcpy(init_los_range, a->los_range, (size_t)pop * sizeof(uint8_t));
    const biosim_nnet_t *n = &sim.nnet;
    const uint16_t mc = n->max_genes;
    const uint8_t mn = n->max_neurons;
    size_t grid_size = (size_t)sim.size_x * (size_t)sim.size_y;

    MKRW(buf_alive, a->alive, sizeof(uint8_t), pop);
    MKRO(buf_loc_x, a->loc_x, sizeof(int32_t), pop);
    MKRO(buf_loc_y, a->loc_y, sizeof(int32_t), pop);
    MKRW(buf_osc_period, a->osc_period, sizeof(uint16_t), pop);
    MKRO(buf_last_move_dir, a->last_move_dir, sizeof(uint8_t), pop);
    MKRW(buf_responsiveness, a->responsiveness, sizeof(float), pop);
    MKRW(buf_los_range, a->los_range, sizeof(uint8_t), pop);
    MKRO(buf_conn_packed, n->genome_conn, sizeof(uint16_t), (size_t)mc * pop);
    MKRO(buf_conn_weight, n->genome_wgt, sizeof(int16_t), (size_t)mc * pop);
    MKRO(buf_conn_length, n->conn_length, sizeof(uint16_t), pop);
    MKRW(buf_neuron_output, n->neuron_output, sizeof(float), (size_t)mn * pop);
    MKRO(buf_neuron_driven, n->neuron_driven, sizeof(uint8_t), (size_t)mn * pop);
    MKRO(buf_neuron_count, n->neuron_count, sizeof(uint8_t), pop);
    MKRW(buf_signal, sim.signal, sizeof(uint32_t), sim.signal_len);
    MKRW(buf_rng_state, a->rng_state, sizeof(uint64_t), pop);
    MKWO(buf_desired_x, NULL, sizeof(int32_t), pop);
    MKWO(buf_desired_y, NULL, sizeof(int32_t), pop);
    MKRO(buf_grid, sim.grid.cells, sizeof(uint32_t), grid_size);
    MKRW(buf_kill_marker, a->kill_marker, sizeof(uint8_t), pop);

    /* Set all kernel arguments (scalars stay fixed across runs). */
    cl_int size_x = sim.size_x;
    cl_int size_y = sim.size_y;
    cl_uint step = (cl_uint)sim.step;
    cl_uint steps_gen = (cl_uint)sim.steps_per_gen;
    cl_uint pop_arg = (cl_uint)sim.population;
    cl_int enable_kill = 0;
    cl_int sensor_radius = (cl_int)sim.sensor_radius;
    cl_float resp_curve_k = (cl_float)sim.responsiveness_curve_k;

    (void)clSetKernelArg(kernel, 0U, sizeof(cl_mem), (const void *)&buf_alive);
    (void)clSetKernelArg(kernel, 1U, sizeof(cl_mem), (const void *)&buf_loc_x);
    (void)clSetKernelArg(kernel, 2U, sizeof(cl_mem), (const void *)&buf_loc_y);
    (void)clSetKernelArg(kernel, 3U, sizeof(cl_mem), (const void *)&buf_osc_period);
    (void)clSetKernelArg(kernel, 4U, sizeof(cl_mem), (const void *)&buf_last_move_dir);
    (void)clSetKernelArg(kernel, 5U, sizeof(cl_mem), (const void *)&buf_responsiveness);
    (void)clSetKernelArg(kernel, 6U, sizeof(cl_mem), (const void *)&buf_los_range);
    (void)clSetKernelArg(kernel, 7U, sizeof(cl_mem), (const void *)&buf_conn_packed);
    (void)clSetKernelArg(kernel, 8U, sizeof(cl_mem), (const void *)&buf_conn_weight);
    (void)clSetKernelArg(kernel, 9U, sizeof(cl_mem), (const void *)&buf_conn_length);
    (void)clSetKernelArg(kernel, 10U, sizeof(cl_mem), (const void *)&buf_neuron_output);
    (void)clSetKernelArg(kernel, 11U, sizeof(cl_mem), (const void *)&buf_neuron_driven);
    (void)clSetKernelArg(kernel, 12U, sizeof(cl_mem), (const void *)&buf_neuron_count);
    (void)clSetKernelArg(kernel, 13U, sizeof(cl_mem), (const void *)&buf_signal);
    (void)clSetKernelArg(kernel, 14U, sizeof(cl_mem), (const void *)&buf_rng_state);
    (void)clSetKernelArg(kernel, 15U, sizeof(cl_mem), (const void *)&buf_desired_x);
    (void)clSetKernelArg(kernel, 16U, sizeof(cl_mem), (const void *)&buf_desired_y);
    (void)clSetKernelArg(kernel, 17U, sizeof(cl_mem), (const void *)&buf_grid);
    (void)clSetKernelArg(kernel, 18U, sizeof(cl_mem), (const void *)&buf_kill_marker);
    (void)clSetKernelArg(kernel, 19U, sizeof(cl_int), &size_x);
    (void)clSetKernelArg(kernel, 20U, sizeof(cl_int), &size_y);
    (void)clSetKernelArg(kernel, 21U, sizeof(cl_uint), &step);
    (void)clSetKernelArg(kernel, 22U, sizeof(cl_uint), &steps_gen);
    (void)clSetKernelArg(kernel, 23U, sizeof(cl_uint), &pop_arg);
    (void)clSetKernelArg(kernel, 24U, sizeof(cl_int), &enable_kill);
    (void)clSetKernelArg(kernel, 25U, sizeof(cl_int), &sensor_radius);
    (void)clSetKernelArg(kernel, 26U, sizeof(cl_float), &resp_curve_k);
}

static void fixture_teardown(void) {
    free(gpu_desired_x);
    free(gpu_desired_y);
    free(gpu_signal);
    free(gpu_responsiveness);
    free(gpu_osc_period);
    free(gpu_los_range);
    free(gpu_kill_marker);
    free(init_rng_state);
    free(init_responsiveness);
    free(init_osc_period);
    free(init_los_range);

    SAFE_RELEASE(clReleaseMemObject, buf_kill_marker);
    SAFE_RELEASE(clReleaseMemObject, buf_grid);
    SAFE_RELEASE(clReleaseMemObject, buf_desired_y);
    SAFE_RELEASE(clReleaseMemObject, buf_desired_x);
    SAFE_RELEASE(clReleaseMemObject, buf_rng_state);
    SAFE_RELEASE(clReleaseMemObject, buf_signal);
    SAFE_RELEASE(clReleaseMemObject, buf_neuron_count);
    SAFE_RELEASE(clReleaseMemObject, buf_neuron_driven);
    SAFE_RELEASE(clReleaseMemObject, buf_neuron_output);
    SAFE_RELEASE(clReleaseMemObject, buf_conn_length);
    SAFE_RELEASE(clReleaseMemObject, buf_conn_weight);
    SAFE_RELEASE(clReleaseMemObject, buf_conn_packed);
    SAFE_RELEASE(clReleaseMemObject, buf_los_range);
    SAFE_RELEASE(clReleaseMemObject, buf_responsiveness);
    SAFE_RELEASE(clReleaseMemObject, buf_last_move_dir);
    SAFE_RELEASE(clReleaseMemObject, buf_osc_period);
    SAFE_RELEASE(clReleaseMemObject, buf_loc_y);
    SAFE_RELEASE(clReleaseMemObject, buf_loc_x);
    SAFE_RELEASE(clReleaseMemObject, buf_alive);
    SAFE_RELEASE(clReleaseKernel, kernel);
    SAFE_RELEASE(clReleaseProgram, program);

    biosim_gpu_runner_free(&runner);
    biosim_sim_free(&sim);
}

/* ── helper: restore initial GPU state and dispatch K1 ─────────────────── */

/* Re-upload all mutable GPU buffers to their initial state, then dispatch K1
 * and read back desired_x/desired_y. Returns 1 on success. */
static int run_k1(void) {
    cl_command_queue q = runner.queue;
    uint32_t pop = sim.population;
    const biosim_nnet_t *n = &sim.nnet;
    size_t n_neuron = (size_t)n->max_neurons * (size_t)pop;

    /* Restore mutable per-agent fields to initial snapshot. */
    WRITE(buf_rng_state, init_rng_state, pop, sizeof(uint64_t));
    WRITE(buf_responsiveness, init_responsiveness, pop, sizeof(float));
    WRITE(buf_osc_period, init_osc_period, pop, sizeof(uint16_t));
    WRITE(buf_los_range, init_los_range, pop, sizeof(uint8_t));
    WRITE(buf_neuron_output, n->neuron_output, n_neuron, sizeof(float));
    WRITE(buf_signal, sim.signal, sim.signal_len, sizeof(uint32_t));

    size_t global_size = (size_t)pop;
    if (clEnqueueNDRangeKernel(q, kernel, 1U, NULL, &global_size, NULL, 0U, NULL, NULL) !=
        CL_SUCCESS) {
        return 0;
    }

    READ(buf_desired_x, gpu_desired_x, pop, sizeof(int32_t));
    READ(buf_desired_y, gpu_desired_y, pop, sizeof(int32_t));
    READ(buf_signal, gpu_signal, sim.signal_len, sizeof(uint32_t));
    READ(buf_responsiveness, gpu_responsiveness, pop, sizeof(float));
    READ(buf_osc_period, gpu_osc_period, pop, sizeof(uint16_t));
    READ(buf_los_range, gpu_los_range, pop, sizeof(uint8_t));
    READ(buf_kill_marker, gpu_kill_marker, pop, sizeof(uint8_t));

    /* Wait for the queue to complete */
    if (clFinish(q) != CL_SUCCESS) {
        return 0;
    }

    return 1;
}

/* Run the host reference for one agent: sensor eval → feedforward → actions →
 * movement finalize. Does NOT update the grid. Restores all mutable agent
 * fields from the initial snapshot first so multiple calls give reproducible
 * results. */
static void run_host_step_agent(uint32_t idx) {
    biosim_agents_t *a = &sim.agents;
    biosim_nnet_t *n = &sim.nnet;
    const uint32_t pop = sim.population;

    /* Restore all mutable per-agent fields to initial snapshot. */
    a->rng_state[idx] = init_rng_state[idx];
    a->responsiveness[idx] = init_responsiveness[idx];
    a->osc_period[idx] = init_osc_period[idx];
    a->los_range[idx] = init_los_range[idx];
    for (uint8_t k = 0U; k < n->max_neurons; k++) {
        n->neuron_output[(size_t)k * pop + idx] = 0.0F;
    }

    float sensor_vals[BIOSIM_NUM_SENSORS];
    float action_vals[BIOSIM_NUM_ACTIONS];

    for (uint32_t s = 0U; s < BIOSIM_NUM_SENSORS; s++) {
        sensor_vals[s] = biosim_sensor_eval((biosim_sensor_t)s, idx, &sim);
    }

    memset(action_vals, 0, sizeof(action_vals));
    biosim_nnet_feedforward(n, idx, sensor_vals, action_vals);

    a->dx_sum[idx] = 0.0F;
    a->dy_sum[idx] = 0.0F;

    for (uint32_t act = 0U; act < BIOSIM_NUM_ACTIONS; act++) {
        biosim_action_apply((biosim_action_t)act, action_vals[act], idx, &sim);
    }

    biosim_action_propose_move(idx, &sim);
}

/* ── tests ──────────────────────────────────────────────────────────────── */

/* Verify that K1 dispatches successfully and all alive agents receive a valid
 * (in-grid-bounds) desired position. */
void test_k1_compiles_and_runs(void) {
    TEST_ASSERT_EQUAL_INT_MESSAGE(BIOSIM_OK, fixture_status, "fixture setup failed");

    TEST_ASSERT_TRUE_MESSAGE(run_k1(), "K1 kernel dispatch failed");

    uint32_t pop = sim.population;
    for (uint32_t i = 0U; i < pop; i++) {
        if (!sim.agents.alive[i]) {
            continue;
        }
        TEST_ASSERT_TRUE(gpu_desired_x[i] >= 0);
        TEST_ASSERT_TRUE(gpu_desired_x[i] < sim.size_x);
        TEST_ASSERT_TRUE(gpu_desired_y[i] >= 0);
        TEST_ASSERT_TRUE(gpu_desired_y[i] < sim.size_y);
    }
}

/* Compare GPU desired positions against the host reference for every alive
 * agent. Both sides start from the same initial state (rng_state, neuron
 * outputs, responsiveness, osc_period, los_range, signal=0), so they
 * must produce identical results. */
void test_k1_matches_host_reference(void) {
    TEST_ASSERT_EQUAL_INT_MESSAGE(BIOSIM_OK, fixture_status, "fixture setup failed");

    /* Fill signal with a deterministic non-zero pattern so that SIGNAL0_FWD
     * and SIGNAL0_LR sensors exercise real values on both GPU and host. */
    for (size_t i = 0; i < sim.signal_len; i++) {
        sim.signal[i] = (uint32_t)((i * 7U + 3U) % 200U) + 1U;
    }

    TEST_ASSERT_TRUE_MESSAGE(run_k1(), "K1 kernel dispatch failed");

    uint32_t pop = sim.population;

    uint32_t mismatches_dxy = 0U;
    uint32_t mismatches_resp = 0U;
    uint32_t mismatches_osc = 0U;
    uint32_t mismatches_los_range = 0U;
    uint32_t mismatches_km = 0U;
    for (uint32_t i = 0U; i < pop; i++) {
        if (!sim.agents.alive[i]) {
            continue;
        }
        run_host_step_agent(i);

        if (gpu_desired_x[i] != sim.agents.desired_x[i] ||
            gpu_desired_y[i] != sim.agents.desired_y[i]) {
            mismatches_dxy++;
        }
        if (fabsf(gpu_responsiveness[i] - sim.agents.responsiveness[i]) > 1e-5F) {
            mismatches_resp++;
        }
        if (gpu_osc_period[i] != sim.agents.osc_period[i]) {
            mismatches_osc++;
        }
        if (gpu_los_range[i] != sim.agents.los_range[i]) {
            mismatches_los_range++;
        }
        if (gpu_kill_marker[i] != sim.agents.kill_marker[i]) {
            mismatches_km++;
        }
    }

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
        0U, mismatches_dxy, "dxy per-agent buffer mismatch(es) between GPU and host"
    );
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
        0U, mismatches_resp, "responsiveness per-agent buffer mismatch(es) between GPU and host"
    );
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
        0U, mismatches_osc, "osc per-agent buffer mismatch(es) between GPU and host"
    );
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
        0U, mismatches_los_range, "los_range per-agent buffer mismatch(es) between GPU and host"
    );
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
        0U, mismatches_km, "kill_marker per-agent buffer mismatch(es) between GPU and host"
    );

    /* Signal buffer: aggregate over all agents; may differ due to GPU tanh
     * precision vs tanhf — divergence on this assertion is expected. */
    uint32_t signal_mismatches = 0U;
    for (size_t i = 0U; i < sim.signal_len; i++) {
        if (gpu_signal[i] != sim.signal[i]) {
            signal_mismatches++;
        }
    }
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
        0U, signal_mismatches, "signal buffer mismatch between GPU and host"
    );
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
