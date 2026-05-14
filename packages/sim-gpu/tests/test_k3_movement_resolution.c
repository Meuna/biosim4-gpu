#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/opencl.h>
#endif

#include "biosim/core/grid.h"
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

/* GPU buffers for K3 inputs/outputs. */
static cl_mem g_buf_alive;
static cl_mem g_buf_desired_x;
static cl_mem g_buf_desired_y;
static cl_mem g_buf_loc_x;
static cl_mem g_buf_loc_y;
static cl_mem g_buf_last_move_dir;
static cl_mem g_buf_grid;

/* Host readback arrays (allocated in fixture_setup, sized to population). */
static int32_t *g_result_loc_x;
static int32_t *g_result_loc_y;
static uint8_t *g_result_last_move_dir;
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
    if (biosim_gpu_registry_get("k3_movement_resolution", NULL, &sources) != BIOSIM_OK) {
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
    g_kernel = clCreateKernel(g_program, "k_movement_resolution", &cl_err);
    if (cl_err != CL_SUCCESS || !g_kernel) {
        g_opencl_ok = 0;
        return;
    }

    uint32_t pop = g_sim.population;
    size_t grid_cells = (size_t)g_sim.size_x * (size_t)g_sim.size_y;

    g_result_loc_x = malloc((size_t)pop * sizeof(int32_t));
    g_result_loc_y = malloc((size_t)pop * sizeof(int32_t));
    g_result_last_move_dir = malloc((size_t)pop * sizeof(uint8_t));
    g_result_grid = malloc(grid_cells * sizeof(uint32_t));

    if (!g_result_loc_x || !g_result_loc_y || !g_result_last_move_dir || !g_result_grid) {
        g_opencl_ok = 0;
        return;
    }

    /* Allocate GPU buffers (content uploaded per-test via run_k3). */
#define MKRW(var, esz, cnt)                                                                        \
    do {                                                                                           \
        (var) = clCreateBuffer(g_runner.context, CL_MEM_READ_WRITE, (size_t)(cnt) * (esz), NULL,   \
                               &cl_err);                                                           \
        if (cl_err != CL_SUCCESS || !(var)) {                                                      \
            g_opencl_ok = 0;                                                                       \
            return;                                                                                \
        }                                                                                          \
    } while (0)

    MKRW(g_buf_alive, sizeof(uint8_t), pop);
    MKRW(g_buf_desired_x, sizeof(int32_t), pop);
    MKRW(g_buf_desired_y, sizeof(int32_t), pop);
    MKRW(g_buf_loc_x, sizeof(int32_t), pop);
    MKRW(g_buf_loc_y, sizeof(int32_t), pop);
    MKRW(g_buf_last_move_dir, sizeof(uint8_t), pop);
    MKRW(g_buf_grid, sizeof(uint32_t), grid_cells);

#undef MKRW

    /* Set fixed kernel arguments (same across all tests). */
    cl_int size_x = g_sim.size_x;
    cl_int size_y = g_sim.size_y;
    cl_uint pop_arg = (cl_uint)g_sim.population;

    (void)clSetKernelArg(g_kernel, 0U, sizeof(cl_mem), (const void *)&g_buf_alive);
    (void)clSetKernelArg(g_kernel, 1U, sizeof(cl_mem), (const void *)&g_buf_desired_x);
    (void)clSetKernelArg(g_kernel, 2U, sizeof(cl_mem), (const void *)&g_buf_desired_y);
    (void)clSetKernelArg(g_kernel, 3U, sizeof(cl_mem), (const void *)&g_buf_loc_x);
    (void)clSetKernelArg(g_kernel, 4U, sizeof(cl_mem), (const void *)&g_buf_loc_y);
    (void)clSetKernelArg(g_kernel, 5U, sizeof(cl_mem), (const void *)&g_buf_last_move_dir);
    (void)clSetKernelArg(g_kernel, 6U, sizeof(cl_mem), (const void *)&g_buf_grid);
    (void)clSetKernelArg(g_kernel, 7U, sizeof(cl_int), &size_x);
    (void)clSetKernelArg(g_kernel, 8U, sizeof(cl_int), &size_y);
    (void)clSetKernelArg(g_kernel, 9U, sizeof(cl_uint), &pop_arg);
}

static void fixture_teardown(void) {
    free(g_result_loc_x);
    free(g_result_loc_y);
    free(g_result_last_move_dir);
    free(g_result_grid);
    g_result_loc_x = NULL;
    g_result_loc_y = NULL;
    g_result_last_move_dir = NULL;
    g_result_grid = NULL;

#define REL(v)                                                                                     \
    do {                                                                                           \
        if (v) {                                                                                   \
            clReleaseMemObject(v);                                                                 \
            (v) = NULL;                                                                            \
        }                                                                                          \
    } while (0)
    REL(g_buf_grid);
    REL(g_buf_last_move_dir);
    REL(g_buf_loc_y);
    REL(g_buf_loc_x);
    REL(g_buf_desired_y);
    REL(g_buf_desired_x);
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

/* ── scenario upload + dispatch + readback ──────────────────────────────── */

typedef struct {
    const uint8_t *alive;
    const int32_t *loc_x;
    const int32_t *loc_y;
    const int32_t *desired_x;
    const int32_t *desired_y;
    const uint8_t *last_move_dir;
    const uint32_t *grid; /* uint32_t, size_x * size_y */
} k3_scenario_t;

/* Upload scenario, dispatch K3, read back results.  Returns 1 on success. */
static int run_k3(const k3_scenario_t *s) {
    cl_command_queue q = g_runner.queue;
    uint32_t pop = g_sim.population;
    size_t grid_cells = (size_t)g_sim.size_x * (size_t)g_sim.size_y;

#define WR(buf, ptr, esz, cnt)                                                                     \
    if (clEnqueueWriteBuffer(q, (buf), CL_FALSE, 0U, (size_t)(cnt) * (esz), (ptr), 0U, NULL,       \
                             NULL) != CL_SUCCESS) {                                                \
        return 0;                                                                                  \
    }

    WR(g_buf_alive, s->alive, sizeof(uint8_t), pop);
    WR(g_buf_loc_x, s->loc_x, sizeof(int32_t), pop);
    WR(g_buf_loc_y, s->loc_y, sizeof(int32_t), pop);
    WR(g_buf_desired_x, s->desired_x, sizeof(int32_t), pop);
    WR(g_buf_desired_y, s->desired_y, sizeof(int32_t), pop);
    WR(g_buf_last_move_dir, s->last_move_dir, sizeof(uint8_t), pop);
    WR(g_buf_grid, s->grid, sizeof(uint32_t), grid_cells);

#undef WR

    size_t global_size = (size_t)pop;
    if (clEnqueueNDRangeKernel(q, g_kernel, 1U, NULL, &global_size, NULL, 0U, NULL, NULL) !=
        CL_SUCCESS) {
        return 0;
    }

#define RD(buf, ptr, esz, cnt)                                                                     \
    if (clEnqueueReadBuffer(q, (buf), CL_FALSE, 0U, (size_t)(cnt) * (esz), (ptr), 0U, NULL,        \
                            NULL) != CL_SUCCESS) {                                                 \
        return 0;                                                                                  \
    }

    RD(g_buf_loc_x, g_result_loc_x, sizeof(int32_t), pop);
    RD(g_buf_loc_y, g_result_loc_y, sizeof(int32_t), pop);
    RD(g_buf_last_move_dir, g_result_last_move_dir, sizeof(uint8_t), pop);

#undef RD

    if (clEnqueueReadBuffer(q, g_buf_grid, CL_TRUE, 0U, grid_cells * sizeof(uint32_t),
                            g_result_grid, 0U, NULL, NULL) != CL_SUCCESS) {
        return 0;
    }
    return 1;
}

/* ── tests ──────────────────────────────────────────────────────────────── */

/* Verify K3 compiles and dispatches without error. */
void test_k3_compiles_and_runs(void) {
    if (!g_opencl_ok) {
        TEST_IGNORE_MESSAGE("OpenCL not available");
    }

    TEST_ASSERT_NOT_NULL(g_kernel);

    uint32_t pop = g_sim.population;
    size_t grid_cells = (size_t)g_sim.size_x * (size_t)g_sim.size_y;

    uint8_t *alive = calloc(pop, sizeof(uint8_t));
    int32_t *loc_x = calloc(pop, sizeof(int32_t));
    int32_t *loc_y = calloc(pop, sizeof(int32_t));
    int32_t *desired_x = calloc(pop, sizeof(int32_t));
    int32_t *desired_y = calloc(pop, sizeof(int32_t));
    uint8_t *lmd = calloc(pop, sizeof(uint8_t));
    uint32_t *grid = calloc(grid_cells, sizeof(uint32_t));

    TEST_ASSERT_NOT_NULL(alive);
    TEST_ASSERT_NOT_NULL(loc_x);
    TEST_ASSERT_NOT_NULL(loc_y);
    TEST_ASSERT_NOT_NULL(desired_x);
    TEST_ASSERT_NOT_NULL(desired_y);
    TEST_ASSERT_NOT_NULL(lmd);
    TEST_ASSERT_NOT_NULL(grid);

    k3_scenario_t s = {alive, loc_x, loc_y, desired_x, desired_y, lmd, grid};
    TEST_ASSERT_TRUE_MESSAGE(run_k3(&s), "K3 kernel dispatch failed");

    free(alive);
    free(loc_x);
    free(loc_y);
    free(desired_x);
    free(desired_y);
    free(lmd);
    free(grid);
}

/* An alive agent whose desired position is an empty adjacent cell must move
 * there.  Agent 0 is at (3,3) and desires (4,3) — an EAST step (direction 0).
 * After K3: loc_x[0]==4, grid[(3)*8+(4)]==1, grid[(3)*8+(3)]==0,
 * last_move_dir[0]==0 (EAST). */
void test_k3_empty_cell_move(void) {
    if (!g_opencl_ok) {
        TEST_IGNORE_MESSAGE("OpenCL not available");
    }

    uint32_t pop = g_sim.population;
    int sx = (int)g_sim.size_x;
    size_t grid_cells = (size_t)g_sim.size_x * (size_t)g_sim.size_y;

    uint8_t alive[4] = {1, 0, 0, 0};
    int32_t loc_x[4] = {3, 0, 0, 0};
    int32_t loc_y[4] = {3, 0, 0, 0};
    int32_t desired_x[4] = {4, 0, 0, 0}; /* EAST */
    int32_t desired_y[4] = {3, 0, 0, 0};
    uint8_t lmd[4] = {0, 0, 0, 0};
    uint32_t grid[64]; /* 8*8 */
    memset(grid, 0, sizeof(grid));
    grid[(size_t)(3 * sx + 3)] = 1U; /* agent 0 (1-based) at (3,3) */

    (void)pop;

    k3_scenario_t s = {alive, loc_x, loc_y, desired_x, desired_y, lmd, grid};
    TEST_ASSERT_TRUE_MESSAGE(run_k3(&s), "K3 kernel dispatch failed");

    (void)grid_cells;

    TEST_ASSERT_EQUAL_INT32_MESSAGE(4, g_result_loc_x[0], "loc_x not updated after move");
    TEST_ASSERT_EQUAL_INT32_MESSAGE(3, g_result_loc_y[0], "loc_y changed unexpectedly");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0U, g_result_last_move_dir[0], "last_move_dir should be EAST");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1U, g_result_grid[(size_t)(3 * sx + 4)],
                                     "target cell should contain agent");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0U, g_result_grid[(size_t)(3 * sx + 3)],
                                     "old cell should be empty");
}

/* An agent whose desired position equals its current position must not move and
 * must leave last_move_dir unchanged. */
void test_k3_no_move_when_at_desired(void) {
    if (!g_opencl_ok) {
        TEST_IGNORE_MESSAGE("OpenCL not available");
    }

    int sx = (int)g_sim.size_x;

    uint8_t alive[4] = {1, 0, 0, 0};
    int32_t loc_x[4] = {3, 0, 0, 0};
    int32_t loc_y[4] = {3, 0, 0, 0};
    int32_t desired_x[4] = {3, 0, 0, 0}; /* same as loc */
    int32_t desired_y[4] = {3, 0, 0, 0};
    uint8_t lmd[4] = {2, 0, 0, 0}; /* arbitrary initial direction */
    uint32_t grid[64];
    memset(grid, 0, sizeof(grid));
    grid[(size_t)(3 * sx + 3)] = 1U;

    k3_scenario_t s = {alive, loc_x, loc_y, desired_x, desired_y, lmd, grid};
    TEST_ASSERT_TRUE_MESSAGE(run_k3(&s), "K3 kernel dispatch failed");

    TEST_ASSERT_EQUAL_INT32_MESSAGE(3, g_result_loc_x[0], "loc_x should be unchanged");
    TEST_ASSERT_EQUAL_INT32_MESSAGE(3, g_result_loc_y[0], "loc_y should be unchanged");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(2U, g_result_last_move_dir[0],
                                    "last_move_dir should be unchanged");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1U, g_result_grid[(size_t)(3 * sx + 3)],
                                     "grid cell should still hold agent");
}

/* An agent that desires a cell already occupied by another agent must stay.
 * Agent 0 is at (3,3) desiring (4,3); agent 1 is at (4,3) not moving.
 * After K3: agent 0 is still at (3,3); (4,3) still contains agent 1. */
void test_k3_occupied_cell_blocked(void) {
    if (!g_opencl_ok) {
        TEST_IGNORE_MESSAGE("OpenCL not available");
    }

    int sx = (int)g_sim.size_x;

    uint8_t alive[4] = {1, 1, 0, 0};
    int32_t loc_x[4] = {3, 4, 0, 0};
    int32_t loc_y[4] = {3, 3, 0, 0};
    int32_t desired_x[4] = {4, 4, 0, 0}; /* agent 0 wants (4,3); agent 1 stays */
    int32_t desired_y[4] = {3, 3, 0, 0};
    uint8_t lmd[4] = {0, 0, 0, 0};
    uint32_t grid[64];
    memset(grid, 0, sizeof(grid));
    grid[(size_t)(3 * sx + 3)] = 1U; /* agent 0 */
    grid[(size_t)(3 * sx + 4)] = 2U; /* agent 1 */

    k3_scenario_t s = {alive, loc_x, loc_y, desired_x, desired_y, lmd, grid};
    TEST_ASSERT_TRUE_MESSAGE(run_k3(&s), "K3 kernel dispatch failed");

    TEST_ASSERT_EQUAL_INT32_MESSAGE(3, g_result_loc_x[0], "agent 0 should be blocked");
    TEST_ASSERT_EQUAL_INT32_MESSAGE(3, g_result_loc_y[0], "agent 0 should be blocked");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(2U, g_result_grid[(size_t)(3 * sx + 4)],
                                     "occupied cell should still hold agent 1");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1U, g_result_grid[(size_t)(3 * sx + 3)],
                                     "agent 0 old cell should still hold agent 0");
}

/* ── main ───────────────────────────────────────────────────────────────── */

int main(void) {
    UNITY_BEGIN();
    fixture_setup();
    RUN_TEST(test_k3_compiles_and_runs);
    RUN_TEST(test_k3_empty_cell_move);
    RUN_TEST(test_k3_no_move_when_at_desired);
    RUN_TEST(test_k3_occupied_cell_blocked);
    fixture_teardown();
    return UNITY_END();
}
