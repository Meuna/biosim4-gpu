#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "biosim/core/grid_defs.h"
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

/* GPU buffers for K2 inputs/outputs. */
static cl_mem buf_kill_marker;
static cl_mem buf_loc_x;
static cl_mem buf_loc_y;
static cl_mem buf_grid;

/* Host readback. */
static uint32_t *result_grid;

/* ── Unity setUp / tearDown ─────────────────────────────────────────────── */

void setUp(void) {
}
void tearDown(void) {
}

/* ── global fixture setup / teardown ────────────────────────────────────── */

static void fixture_setup(void) {
    biosim_log_init(&biosim_log_default_ctx);

    fixture_status = sim_test_make_8x8(&sim);
    if (fixture_status != BIOSIM_OK) {
        return;
    }

    fixture_status = gpu_test_kernel_runtime_create(
        &runner, &program, &kernel, "k2_kill_marked", "k_kill_marked"
    );
    if (fixture_status != BIOSIM_OK) {
        return;
    }

    uint32_t pop = sim.population;
    size_t grid_size = (size_t)sim.size_x * (size_t)sim.size_y;

    /* Allocate host-side readback and snapshot buffers. */
    ALLOC(result_grid, grid_size, sizeof(uint32_t));

    /* Allocate GPU buffers; content uploaded per-test. */
    MKRW(buf_kill_marker, NULL, sizeof(uint8_t), pop);
    MKRW(buf_loc_x, NULL, sizeof(int32_t), pop);
    MKRW(buf_loc_y, NULL, sizeof(int32_t), pop);
    MKRW(buf_grid, NULL, sizeof(uint32_t), grid_size);

    /* Set fixed kernel arguments (same across all tests). */
    cl_int size_x = sim.size_x;
    cl_uint pop_arg = (cl_uint)sim.population;

    (void)clSetKernelArg(kernel, 0U, sizeof(cl_mem), (const void *)&buf_kill_marker);
    (void)clSetKernelArg(kernel, 1U, sizeof(cl_mem), (const void *)&buf_loc_x);
    (void)clSetKernelArg(kernel, 2U, sizeof(cl_mem), (const void *)&buf_loc_y);
    (void)clSetKernelArg(kernel, 3U, sizeof(cl_mem), (const void *)&buf_grid);
    (void)clSetKernelArg(kernel, 4U, sizeof(cl_int), &size_x);
    (void)clSetKernelArg(kernel, 5U, sizeof(cl_uint), &pop_arg);
}

static void fixture_teardown(void) {
    free(result_grid);

    SAFE_RELEASE(clReleaseMemObject, buf_grid);
    SAFE_RELEASE(clReleaseMemObject, buf_loc_y);
    SAFE_RELEASE(clReleaseMemObject, buf_loc_x);
    SAFE_RELEASE(clReleaseMemObject, buf_kill_marker);
    SAFE_RELEASE(clReleaseKernel, kernel);
    SAFE_RELEASE(clReleaseProgram, program);

    biosim_gpu_runner_free(&runner);
    biosim_sim_free(&sim);
}

/* ── scenario upload + dispatch + readback ──────────────────────────────── */

typedef struct {
    const uint8_t *kill_marker;
    const int32_t *loc_x;
    const int32_t *loc_y;
    const uint32_t *grid;
} k2_scn_t;

static int run_k2(const k2_scn_t *s) {
    cl_command_queue q = runner.queue;
    uint32_t pop = sim.population;
    size_t grid_size = (size_t)sim.size_x * (size_t)sim.size_y;

    WRITE(buf_kill_marker, s->kill_marker, pop, sizeof(uint8_t));
    WRITE(buf_loc_x, s->loc_x, pop, sizeof(int32_t));
    WRITE(buf_loc_y, s->loc_y, pop, sizeof(int32_t));
    WRITE(buf_grid, s->grid, grid_size, sizeof(uint32_t));

    size_t global_size = (size_t)pop;
    if (clEnqueueNDRangeKernel(q, kernel, 1U, NULL, &global_size, NULL, 0U, NULL, NULL) !=
        CL_SUCCESS) {
        return 0;
    }

    READ(buf_grid, result_grid, grid_size, sizeof(uint32_t));

    /* Wait for the queue to complete */
    if (clFinish(q) != CL_SUCCESS) {
        return 0;
    }

    return 1;
}

/* ── tests ──────────────────────────────────────────────────────────────── */

/* Verify K2 kill_marked compiles and dispatches without error. */
void test_k2_kill_marked_compiles_and_runs(void) {
    TEST_ASSERT_EQUAL_INT_MESSAGE(BIOSIM_OK, fixture_status, "fixture setup failed");

    uint32_t pop = sim.population;
    size_t grid_size = (size_t)sim.size_x * (size_t)sim.size_y;

    uint8_t *km = calloc_test_assert(pop, sizeof(uint8_t));
    int32_t *lx = calloc_test_assert(pop, sizeof(int32_t));
    int32_t *ly = calloc_test_assert(pop, sizeof(int32_t));
    uint32_t *grid = calloc_test_assert(grid_size, sizeof(uint32_t));

    k2_scn_t s = {km, lx, ly, grid};
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
    TEST_ASSERT_EQUAL_INT_MESSAGE(BIOSIM_OK, fixture_status, "fixture setup failed");

    int sx = (int)sim.size_x;

    uint8_t km[4] = {1, 0, 0, 0}; /* agent 0 marked */
    int32_t lx[4] = {3, 0, 0, 0};
    int32_t ly[4] = {3, 0, 0, 0};
    uint32_t grid[64];
    memset(grid, 0, sizeof(grid));
    grid[(size_t)(3 * sx + 3)] = 1U; /* agent 0 at (3,3) */

    k2_scn_t s = {km, lx, ly, grid};
    TEST_ASSERT_TRUE_MESSAGE(run_k2(&s), "K2 kill_marked dispatch failed");

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
        BIOSIM_GRID_EMPTY,
        result_grid[(size_t)(3 * sx + 3)],
        "kill-marked agent's cell must be cleared"
    );
}

/* An agent with kill_marker=0 must NOT have its grid cell modified.
 * Agent 0 is at (2,2), grid[(2*8+2)] == 1.
 * After K2: cell unchanged. */
void test_k2_kill_marked_skips_unmarked(void) {
    TEST_ASSERT_EQUAL_INT_MESSAGE(BIOSIM_OK, fixture_status, "fixture setup failed");

    int sx = (int)sim.size_x;

    uint8_t km[4] = {0, 0, 0, 0}; /* no agent marked */
    int32_t lx[4] = {2, 0, 0, 0};
    int32_t ly[4] = {2, 0, 0, 0};
    uint32_t grid[64];
    memset(grid, 0, sizeof(grid));
    grid[(size_t)(2 * sx + 2)] = 1U; /* agent 0 at (2,2) */

    k2_scn_t s = {km, lx, ly, grid};
    TEST_ASSERT_TRUE_MESSAGE(run_k2(&s), "K2 kill_marked dispatch failed");

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
        1U, result_grid[(size_t)(2 * sx + 2)], "unmarked agent's cell must be unchanged"
    );
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
