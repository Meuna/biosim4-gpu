#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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

static cl_mem buf_signal;
static uint32_t *result_signal;

/* ── Unity setUp / tearDown ─────────────────────────────────────────────── */

void setUp(void) {
}
void tearDown(void) {
}

/* ── global fixture setup / teardown ────────────────────────────────────── */

static void fixture_setup(void) {
    biosim_log_init(&biosim_log_default_ctx);

    fixture_status = sim_test_create(
        &sim,
        &(sim_test_scn_t){
            .population = 4U,
            .size_x = 8,
            .size_y = 8,
            .max_genes = 2U,
            .max_neurons = 1U,
            .los_range = 4U,
            .steps_per_gen = 100U,
            .sensor_radius = 1,
        }
    );
    if (fixture_status != BIOSIM_OK) {
        return;
    }

    fixture_status = gpu_test_kernel_runtime_create(
        &runner, &program, &kernel, "k4_signal_fade", "k_signal_fade"
    );
    if (fixture_status != BIOSIM_OK) {
        return;
    }

    size_t grid_size = (size_t)sim.size_x * (size_t)sim.size_y;

    /* Allocate host-side readback and snapshot buffers. */
    ALLOC(result_signal, grid_size, sizeof(uint32_t));

    /* Allocate GPU buffers; content uploaded per-test. */
    MKRW(buf_signal, NULL, sizeof(uint32_t), grid_size);

    (void)clSetKernelArg(kernel, 0U, sizeof(cl_mem), (const void *)&buf_signal);
}

static void fixture_teardown(void) {
    free(result_signal);

    SAFE_RELEASE(clReleaseMemObject, buf_signal);
    SAFE_RELEASE(clReleaseKernel, kernel);
    SAFE_RELEASE(clReleaseProgram, program);

    biosim_gpu_runner_free(&runner);
    biosim_sim_free(&sim);
}

/* ── scenario upload + dispatch + readback ──────────────────────────────── */

static int run_k4(const uint32_t *signal_in) {
    cl_command_queue q = runner.queue;
    size_t grid_size = (size_t)sim.size_x * (size_t)sim.size_y;

    WRITE(buf_signal, signal_in, grid_size, sizeof(uint32_t));

    size_t global_size = grid_size;
    if (clEnqueueNDRangeKernel(q, kernel, 1U, NULL, &global_size, NULL, 0U, NULL, NULL) !=
        CL_SUCCESS) {
        return 0;
    }

    READ(buf_signal, result_signal, grid_size, sizeof(uint32_t));

    /* Wait for the queue to complete */
    if (clFinish(q) != CL_SUCCESS) {
        return 0;
    }

    return 1;
}

/* ── tests ──────────────────────────────────────────────────────────────── */

void test_k4_compiles_and_runs(void) {
    TEST_ASSERT_EQUAL_INT_MESSAGE(BIOSIM_OK, fixture_status, "fixture setup failed");

    size_t grid_size = (size_t)sim.size_x * (size_t)sim.size_y;
    uint32_t *signal = calloc_test_assert(grid_size, sizeof(uint32_t));

    TEST_ASSERT_TRUE_MESSAGE(run_k4(signal), "K4 kernel dispatch failed");

    for (size_t i = 0U; i < grid_size; i++) {
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(0U, result_signal[i], "all-zero input must stay zero");
    }

    free(signal);
}

/* Non-zero signal values must each decrease by exactly 1. */
void test_k4_fades_nonzero_signals(void) {
    TEST_ASSERT_EQUAL_INT_MESSAGE(BIOSIM_OK, fixture_status, "fixture setup failed");

    size_t grid_size = (size_t)sim.size_x * (size_t)sim.size_y;
    uint32_t *signal = calloc_test_assert(grid_size, sizeof(uint32_t));

    signal[0] = 5U;
    signal[1] = 10U;
    signal[2] = 255U;

    TEST_ASSERT_TRUE_MESSAGE(run_k4(signal), "K4 kernel dispatch failed");

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(4U, result_signal[0], "5 must fade to 4");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(9U, result_signal[1], "10 must fade to 9");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(254U, result_signal[2], "255 must fade to 254");

    free(signal);
}

/* Zero-valued cells must stay at 0 — no uint underflow to UINT_MAX. */
void test_k4_zero_does_not_underflow(void) {
    TEST_ASSERT_EQUAL_INT_MESSAGE(BIOSIM_OK, fixture_status, "fixture setup failed");

    size_t grid_size = (size_t)sim.size_x * (size_t)sim.size_y;
    uint32_t *signal = calloc_test_assert(grid_size, sizeof(uint32_t));

    signal[0] = 0U;
    signal[1] = 3U;
    signal[2] = 0U;
    signal[3] = 1U;

    TEST_ASSERT_TRUE_MESSAGE(run_k4(signal), "K4 kernel dispatch failed");

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0U, result_signal[0], "0 must not underflow");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(2U, result_signal[1], "3 must fade to 2");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0U, result_signal[2], "0 must not underflow");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0U, result_signal[3], "1 must fade to 0");

    free(signal);
}

/* ── main ───────────────────────────────────────────────────────────────── */

int main(void) {
    UNITY_BEGIN();
    fixture_setup();
    RUN_TEST(test_k4_compiles_and_runs);
    RUN_TEST(test_k4_fades_nonzero_signals);
    RUN_TEST(test_k4_zero_does_not_underflow);
    fixture_teardown();
    return UNITY_END();
}
