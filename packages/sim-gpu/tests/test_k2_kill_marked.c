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
#include "biosim/core/types.h"
#include "biosim/sim-gpu/registry.h"
#include "biosim/sim-gpu/runner.h"
#include "unity.h"

/* ── test fixture ───────────────────────────────────────────────────────── */

static biosim_sim_t g_sim;
static biosim_gpu_runner_t g_runner;
static cl_program g_program;
static cl_kernel g_kernel;
static int g_opencl_ok;

/* GPU buffers for K2 inputs/outputs. */
static cl_mem g_buf_kill_marker;
static cl_mem g_buf_loc_x;
static cl_mem g_buf_loc_y;
static cl_mem g_buf_grid;

/* Host readback. */
static uint32_t *g_result_grid;

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
    if (biosim_gpu_registry_get("k2_kill_marked", NULL, &sources) != BIOSIM_OK) {
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
    g_kernel = clCreateKernel(g_program, "k_kill_marked", &cl_err);
    if (cl_err != CL_SUCCESS || !g_kernel) {
        g_opencl_ok = 0;
        return;
    }

    uint32_t pop = g_sim.population;
    size_t grid_cells = (size_t)g_sim.size_x * (size_t)g_sim.size_y;

    g_result_grid = malloc(grid_cells * sizeof(uint32_t));
    if (!g_result_grid) {
        g_opencl_ok = 0;
        return;
    }

#define MKRW(var, esz, cnt)                                                                        \
    do {                                                                                           \
        (var) = clCreateBuffer(g_runner.context, CL_MEM_READ_WRITE, (size_t)(cnt) * (esz), NULL,   \
                               &cl_err);                                                           \
        if (cl_err != CL_SUCCESS || !(var)) {                                                      \
            g_opencl_ok = 0;                                                                       \
            return;                                                                                \
        }                                                                                          \
    } while (0)

    MKRW(g_buf_kill_marker, sizeof(uint8_t), pop);
    MKRW(g_buf_loc_x, sizeof(int16_t), pop);
    MKRW(g_buf_loc_y, sizeof(int16_t), pop);
    MKRW(g_buf_grid, sizeof(uint32_t), grid_cells);

#undef MKRW

    /* Set fixed kernel arguments (same across all tests). */
    cl_int size_x = (cl_int)g_sim.size_x;
    cl_uint pop_arg = (cl_uint)g_sim.population;

    (void)clSetKernelArg(g_kernel, 0U, sizeof(cl_mem), (const void *)&g_buf_kill_marker);
    (void)clSetKernelArg(g_kernel, 1U, sizeof(cl_mem), (const void *)&g_buf_loc_x);
    (void)clSetKernelArg(g_kernel, 2U, sizeof(cl_mem), (const void *)&g_buf_loc_y);
    (void)clSetKernelArg(g_kernel, 3U, sizeof(cl_mem), (const void *)&g_buf_grid);
    (void)clSetKernelArg(g_kernel, 4U, sizeof(cl_int), &size_x);
    (void)clSetKernelArg(g_kernel, 5U, sizeof(cl_uint), &pop_arg);
}

static void fixture_teardown(void) {
    free(g_result_grid);
    g_result_grid = NULL;

#define REL(v)                                                                                     \
    do {                                                                                           \
        if (v) {                                                                                   \
            clReleaseMemObject(v);                                                                 \
            (v) = NULL;                                                                            \
        }                                                                                          \
    } while (0)
    REL(g_buf_grid);
    REL(g_buf_loc_y);
    REL(g_buf_loc_x);
    REL(g_buf_kill_marker);
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

/* ── scenario upload + dispatch + readback ──────────────────────────────── */

typedef struct {
    const uint8_t *kill_marker;
    const int16_t *loc_x;
    const int16_t *loc_y;
    const uint32_t *grid;
} k2_scenario_t;

static int run_k2(const k2_scenario_t *s) {
    cl_command_queue q = g_runner.queue;
    uint32_t pop = g_sim.population;
    size_t grid_cells = (size_t)g_sim.size_x * (size_t)g_sim.size_y;

#define WR(buf, ptr, esz, cnt)                                                                     \
    if (clEnqueueWriteBuffer(q, (buf), CL_FALSE, 0U, (size_t)(cnt) * (esz), (ptr), 0U, NULL,       \
                             NULL) != CL_SUCCESS) {                                                \
        return 0;                                                                                  \
    }

    WR(g_buf_kill_marker, s->kill_marker, sizeof(uint8_t), pop);
    WR(g_buf_loc_x, s->loc_x, sizeof(int16_t), pop);
    WR(g_buf_loc_y, s->loc_y, sizeof(int16_t), pop);
    WR(g_buf_grid, s->grid, sizeof(uint32_t), grid_cells);

#undef WR

    size_t global_size = (size_t)pop;
    if (clEnqueueNDRangeKernel(q, g_kernel, 1U, NULL, &global_size, NULL, 0U, NULL, NULL) !=
        CL_SUCCESS) {
        return 0;
    }

    if (clEnqueueReadBuffer(q, g_buf_grid, CL_TRUE, 0U, grid_cells * sizeof(uint32_t),
                            g_result_grid, 0U, NULL, NULL) != CL_SUCCESS) {
        return 0;
    }
    return 1;
}

/* ── tests ──────────────────────────────────────────────────────────────── */

/* Verify K2 kill_marked compiles and dispatches without error. */
void test_k2_kill_marked_compiles_and_runs(void) {
    if (!g_opencl_ok) {
        TEST_IGNORE_MESSAGE("OpenCL not available");
    }
    TEST_ASSERT_NOT_NULL(g_kernel);

    uint32_t pop = g_sim.population;
    size_t grid_cells = (size_t)g_sim.size_x * (size_t)g_sim.size_y;

    uint8_t *km = calloc(pop, sizeof(uint8_t));
    int16_t *lx = calloc(pop, sizeof(int16_t));
    int16_t *ly = calloc(pop, sizeof(int16_t));
    uint32_t *grid = calloc(grid_cells, sizeof(uint32_t));

    TEST_ASSERT_NOT_NULL(km);
    TEST_ASSERT_NOT_NULL(lx);
    TEST_ASSERT_NOT_NULL(ly);
    TEST_ASSERT_NOT_NULL(grid);

    k2_scenario_t s = {km, lx, ly, grid};
    TEST_ASSERT_TRUE_MESSAGE(run_k2(&s), "K2 kill_marked dispatch failed");

    free(km);
    free(lx);
    free(ly);
    free(grid);
}

/* An agent with kill_marker=1 must have its grid cell cleared.
 * Agent 0 is at (3,3), grid[(3*8+3)] == 1 (agent 0, 1-based).
 * After K2: grid[(3*8+3)] == BIOSIM_GRID_EMPTY. */
void test_k2_kill_marked_clears_marked_cell(void) {
    if (!g_opencl_ok) {
        TEST_IGNORE_MESSAGE("OpenCL not available");
    }

    int sx = (int)g_sim.size_x;

    uint8_t km[4] = {1, 0, 0, 0}; /* agent 0 marked */
    int16_t lx[4] = {3, 0, 0, 0};
    int16_t ly[4] = {3, 0, 0, 0};
    uint32_t grid[64];
    memset(grid, 0, sizeof(grid));
    grid[(size_t)(3 * sx + 3)] = 1U; /* agent 0 at (3,3) */

    k2_scenario_t s = {km, lx, ly, grid};
    TEST_ASSERT_TRUE_MESSAGE(run_k2(&s), "K2 kill_marked dispatch failed");

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(BIOSIM_GRID_EMPTY, g_result_grid[(size_t)(3 * sx + 3)],
                                     "kill-marked agent's cell must be cleared");
}

/* An agent with kill_marker=0 must NOT have its grid cell modified.
 * Agent 0 is at (2,2), grid[(2*8+2)] == 1.
 * After K2: cell unchanged. */
void test_k2_kill_marked_skips_unmarked(void) {
    if (!g_opencl_ok) {
        TEST_IGNORE_MESSAGE("OpenCL not available");
    }

    int sx = (int)g_sim.size_x;

    uint8_t km[4] = {0, 0, 0, 0}; /* no agent marked */
    int16_t lx[4] = {2, 0, 0, 0};
    int16_t ly[4] = {2, 0, 0, 0};
    uint32_t grid[64];
    memset(grid, 0, sizeof(grid));
    grid[(size_t)(2 * sx + 2)] = 1U; /* agent 0 at (2,2) */

    k2_scenario_t s = {km, lx, ly, grid};
    TEST_ASSERT_TRUE_MESSAGE(run_k2(&s), "K2 kill_marked dispatch failed");

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1U, g_result_grid[(size_t)(2 * sx + 2)],
                                     "unmarked agent's cell must be unchanged");
}

/* ── main ───────────────────────────────────────────────────────────────── */

int main(void) {
    UNITY_BEGIN();
    fixture_setup();
    RUN_TEST(test_k2_kill_marked_compiles_and_runs);
    RUN_TEST(test_k2_kill_marked_clears_marked_cell);
    RUN_TEST(test_k2_kill_marked_skips_unmarked);
    fixture_teardown();
    return UNITY_END();
}
