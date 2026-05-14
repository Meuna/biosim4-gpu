#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/opencl.h>
#endif

#include "biosim/core/log.h"
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

static cl_mem g_buf_signal;
static uint32_t *g_result_signal;

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

static void fixture_setup(void) {
    biosim_log_init(&biosim_log_default_ctx);

    g_opencl_ok = opencl_available();
    if (!g_opencl_ok) {
        return;
    }

    memset(&g_sim, 0, sizeof(g_sim));
    g_sim.population = 4U;
    g_sim.size_x = 8;
    g_sim.size_y = 8;
    g_sim.genome_max_len = 2U;
    g_sim.max_neurons = 1U;
    g_sim.long_probe_dist = 4U;
    g_sim.steps_per_gen = 100U;
    g_sim.gen_rng = 1U;

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
    if (biosim_gpu_registry_get("k4_signal_fade", NULL, &sources) != BIOSIM_OK) {
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
    g_kernel = clCreateKernel(g_program, "k_signal_fade", &cl_err);
    if (cl_err != CL_SUCCESS || !g_kernel) {
        g_opencl_ok = 0;
        return;
    }

    size_t grid_cells = (size_t)g_sim.size_x * (size_t)g_sim.size_y;

    g_result_signal = malloc(grid_cells * sizeof(uint32_t));
    if (!g_result_signal) {
        g_opencl_ok = 0;
        return;
    }

    g_buf_signal = clCreateBuffer(g_runner.context, CL_MEM_READ_WRITE,
                                  grid_cells * sizeof(uint32_t), NULL, &cl_err);
    if (cl_err != CL_SUCCESS || !g_buf_signal) {
        g_opencl_ok = 0;
        return;
    }

    (void)clSetKernelArg(g_kernel, 0U, sizeof(cl_mem), (const void *)&g_buf_signal);
}

static void fixture_teardown(void) {
    free(g_result_signal);
    g_result_signal = NULL;

    if (g_buf_signal) {
        clReleaseMemObject(g_buf_signal);
        g_buf_signal = NULL;
    }
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

/* ── scenario upload + dispatch + readback ──────────────────────────────── */

static int run_k4(const uint32_t *signal_in) {
    cl_command_queue q = g_runner.queue;
    size_t grid_cells = (size_t)g_sim.size_x * (size_t)g_sim.size_y;

    if (clEnqueueWriteBuffer(q, g_buf_signal, CL_FALSE, 0U, grid_cells * sizeof(uint32_t),
                             signal_in, 0U, NULL, NULL) != CL_SUCCESS) {
        return 0;
    }

    size_t global_size = grid_cells;
    if (clEnqueueNDRangeKernel(q, g_kernel, 1U, NULL, &global_size, NULL, 0U, NULL, NULL) !=
        CL_SUCCESS) {
        return 0;
    }

    if (clEnqueueReadBuffer(q, g_buf_signal, CL_TRUE, 0U, grid_cells * sizeof(uint32_t),
                            g_result_signal, 0U, NULL, NULL) != CL_SUCCESS) {
        return 0;
    }
    return 1;
}

/* ── tests ──────────────────────────────────────────────────────────────── */

void test_k4_compiles_and_runs(void) {
    if (!g_opencl_ok) {
        TEST_IGNORE_MESSAGE("OpenCL not available");
    }

    TEST_ASSERT_NOT_NULL(g_kernel);

    size_t grid_cells = (size_t)g_sim.size_x * (size_t)g_sim.size_y;
    uint32_t *signal = calloc(grid_cells, sizeof(uint32_t));
    TEST_ASSERT_NOT_NULL(signal);

    TEST_ASSERT_TRUE_MESSAGE(run_k4(signal), "K4 kernel dispatch failed");

    for (size_t i = 0U; i < grid_cells; i++) {
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(0U, g_result_signal[i], "all-zero input must stay zero");
    }

    free(signal);
}

/* Non-zero signal values must each decrease by exactly 1. */
void test_k4_fades_nonzero_signals(void) {
    if (!g_opencl_ok) {
        TEST_IGNORE_MESSAGE("OpenCL not available");
    }

    size_t grid_cells = (size_t)g_sim.size_x * (size_t)g_sim.size_y;
    uint32_t *signal = calloc(grid_cells, sizeof(uint32_t));
    TEST_ASSERT_NOT_NULL(signal);

    signal[0] = 5U;
    signal[1] = 10U;
    signal[2] = 255U;

    TEST_ASSERT_TRUE_MESSAGE(run_k4(signal), "K4 kernel dispatch failed");

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(4U, g_result_signal[0], "5 must fade to 4");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(9U, g_result_signal[1], "10 must fade to 9");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(254U, g_result_signal[2], "255 must fade to 254");

    free(signal);
}

/* Zero-valued cells must stay at 0 — no uint underflow to UINT_MAX. */
void test_k4_zero_does_not_underflow(void) {
    if (!g_opencl_ok) {
        TEST_IGNORE_MESSAGE("OpenCL not available");
    }

    size_t grid_cells = (size_t)g_sim.size_x * (size_t)g_sim.size_y;
    uint32_t *signal = calloc(grid_cells, sizeof(uint32_t));
    TEST_ASSERT_NOT_NULL(signal);

    signal[0] = 0U;
    signal[1] = 3U;
    signal[2] = 0U;
    signal[3] = 1U;

    TEST_ASSERT_TRUE_MESSAGE(run_k4(signal), "K4 kernel dispatch failed");

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0U, g_result_signal[0], "0 must not underflow");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(2U, g_result_signal[1], "3 must fade to 2");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0U, g_result_signal[2], "0 must not underflow");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0U, g_result_signal[3], "1 must fade to 0");

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
