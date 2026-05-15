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

/* GPU buffers for K3 inputs/outputs. */
static cl_mem buf_alive;
static cl_mem buf_desired_x;
static cl_mem buf_desired_y;
static cl_mem buf_loc_x;
static cl_mem buf_loc_y;
static cl_mem buf_last_move_dir;
static cl_mem buf_grid;

/* Host readback arrays (allocated in fixture_setup, sized to population). */
static int32_t *result_loc_x;
static int32_t *result_loc_y;
static uint8_t *result_last_move_dir;
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
        &runner, &program, &kernel, "k3_movement_resolution", "k_movement_resolution"
    );
    if (fixture_status != BIOSIM_OK) {
        return;
    }

    uint32_t pop = sim.population;
    size_t grid_size = (size_t)sim.size_x * (size_t)sim.size_y;

    /* Allocate host-side readback and snapshot buffers. */
    ALLOC(result_loc_x, pop, sizeof(int32_t));
    ALLOC(result_loc_y, pop, sizeof(int32_t));
    ALLOC(result_last_move_dir, pop, sizeof(uint8_t));
    ALLOC(result_grid, grid_size, sizeof(uint32_t));

    /* Allocate GPU buffers; content uploaded per-test. */
    MKRW(buf_alive, NULL, sizeof(uint8_t), pop);
    MKRW(buf_desired_x, NULL, sizeof(int32_t), pop);
    MKRW(buf_desired_y, NULL, sizeof(int32_t), pop);
    MKRW(buf_loc_x, NULL, sizeof(int32_t), pop);
    MKRW(buf_loc_y, NULL, sizeof(int32_t), pop);
    MKRW(buf_last_move_dir, NULL, sizeof(uint8_t), pop);
    MKRW(buf_grid, NULL, sizeof(uint32_t), grid_size);

    /* Set fixed kernel arguments (same across all tests). */
    cl_int size_x = sim.size_x;
    cl_int size_y = sim.size_y;
    cl_uint pop_arg = (cl_uint)sim.population;

    (void)clSetKernelArg(kernel, 0U, sizeof(cl_mem), (const void *)&buf_alive);
    (void)clSetKernelArg(kernel, 1U, sizeof(cl_mem), (const void *)&buf_desired_x);
    (void)clSetKernelArg(kernel, 2U, sizeof(cl_mem), (const void *)&buf_desired_y);
    (void)clSetKernelArg(kernel, 3U, sizeof(cl_mem), (const void *)&buf_loc_x);
    (void)clSetKernelArg(kernel, 4U, sizeof(cl_mem), (const void *)&buf_loc_y);
    (void)clSetKernelArg(kernel, 5U, sizeof(cl_mem), (const void *)&buf_last_move_dir);
    (void)clSetKernelArg(kernel, 6U, sizeof(cl_mem), (const void *)&buf_grid);
    (void)clSetKernelArg(kernel, 7U, sizeof(cl_int), &size_x);
    (void)clSetKernelArg(kernel, 8U, sizeof(cl_int), &size_y);
    (void)clSetKernelArg(kernel, 9U, sizeof(cl_uint), &pop_arg);
}

static void fixture_teardown(void) {
    free(result_loc_x);
    free(result_loc_y);
    free(result_last_move_dir);
    free(result_grid);

    SAFE_RELEASE(clReleaseMemObject, buf_grid);
    SAFE_RELEASE(clReleaseMemObject, buf_last_move_dir);
    SAFE_RELEASE(clReleaseMemObject, buf_loc_y);
    SAFE_RELEASE(clReleaseMemObject, buf_loc_x);
    SAFE_RELEASE(clReleaseMemObject, buf_desired_y);
    SAFE_RELEASE(clReleaseMemObject, buf_desired_x);
    SAFE_RELEASE(clReleaseMemObject, buf_alive);
    SAFE_RELEASE(clReleaseKernel, kernel);
    SAFE_RELEASE(clReleaseProgram, program);

    biosim_gpu_runner_free(&runner);
    biosim_sim_free(&sim);
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
} k3_scn_t;

/* Upload scenario, dispatch K3, read back results.  Returns 1 on success. */
static int run_k3(const k3_scn_t *s) {
    cl_command_queue q = runner.queue;
    uint32_t pop = sim.population;
    size_t grid_size = (size_t)sim.size_x * (size_t)sim.size_y;

    WRITE(buf_alive, s->alive, pop, sizeof(uint8_t));
    WRITE(buf_loc_x, s->loc_x, pop, sizeof(int32_t));
    WRITE(buf_loc_y, s->loc_y, pop, sizeof(int32_t));
    WRITE(buf_desired_x, s->desired_x, pop, sizeof(int32_t));
    WRITE(buf_desired_y, s->desired_y, pop, sizeof(int32_t));
    WRITE(buf_last_move_dir, s->last_move_dir, pop, sizeof(uint8_t));
    WRITE(buf_grid, s->grid, grid_size, sizeof(uint32_t));

    size_t global_size = (size_t)pop;
    if (clEnqueueNDRangeKernel(q, kernel, 1U, NULL, &global_size, NULL, 0U, NULL, NULL) !=
        CL_SUCCESS) {
        return 0;
    }

    READ(buf_loc_x, result_loc_x, pop, sizeof(int32_t));
    READ(buf_loc_y, result_loc_y, pop, sizeof(int32_t));
    READ(buf_last_move_dir, result_last_move_dir, pop, sizeof(uint8_t));
    READ(buf_grid, result_grid, grid_size, sizeof(uint32_t));

    /* Wait for the queue to complete */
    if (clFinish(q) != CL_SUCCESS) {
        return 0;
    }

    return 1;
}

/* ── tests ──────────────────────────────────────────────────────────────── */

/* Verify K3 compiles and dispatches without error. */
void test_k3_compiles_and_runs(void) {
    TEST_ASSERT_EQUAL_INT_MESSAGE(BIOSIM_OK, fixture_status, "fixture setup failed");

    uint32_t pop = sim.population;
    size_t grid_size = (size_t)sim.size_x * (size_t)sim.size_y;

    uint8_t *alive = calloc_test_assert(pop, sizeof(uint8_t));
    int32_t *loc_x = calloc_test_assert(pop, sizeof(int32_t));
    int32_t *loc_y = calloc_test_assert(pop, sizeof(int32_t));
    int32_t *desired_x = calloc_test_assert(pop, sizeof(int32_t));
    int32_t *desired_y = calloc_test_assert(pop, sizeof(int32_t));
    uint8_t *lmd = calloc_test_assert(pop, sizeof(uint8_t));
    uint32_t *grid = calloc_test_assert(grid_size, sizeof(uint32_t));

    k3_scn_t s = {alive, loc_x, loc_y, desired_x, desired_y, lmd, grid};
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
    TEST_ASSERT_EQUAL_INT_MESSAGE(BIOSIM_OK, fixture_status, "fixture setup failed");

    uint32_t pop = sim.population;
    int sx = (int)sim.size_x;
    size_t grid_size = (size_t)sim.size_x * (size_t)sim.size_y;

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

    k3_scn_t s = {alive, loc_x, loc_y, desired_x, desired_y, lmd, grid};
    TEST_ASSERT_TRUE_MESSAGE(run_k3(&s), "K3 kernel dispatch failed");

    (void)grid_size;

    TEST_ASSERT_EQUAL_INT32_MESSAGE(4, result_loc_x[0], "loc_x not updated after move");
    TEST_ASSERT_EQUAL_INT32_MESSAGE(3, result_loc_y[0], "loc_y changed unexpectedly");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0U, result_last_move_dir[0], "last_move_dir should be EAST");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
        1U, result_grid[(size_t)(3 * sx + 4)], "target cell should contain agent"
    );
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
        0U, result_grid[(size_t)(3 * sx + 3)], "old cell should be empty"
    );
}

/* An agent whose desired position equals its current position must not move and
 * must leave last_move_dir unchanged. */
void test_k3_no_move_when_at_desired(void) {
    TEST_ASSERT_EQUAL_INT_MESSAGE(BIOSIM_OK, fixture_status, "fixture setup failed");

    int sx = (int)sim.size_x;

    uint8_t alive[4] = {1, 0, 0, 0};
    int32_t loc_x[4] = {3, 0, 0, 0};
    int32_t loc_y[4] = {3, 0, 0, 0};
    int32_t desired_x[4] = {3, 0, 0, 0}; /* same as loc */
    int32_t desired_y[4] = {3, 0, 0, 0};
    uint8_t lmd[4] = {2, 0, 0, 0}; /* arbitrary initial direction */
    uint32_t grid[64];
    memset(grid, 0, sizeof(grid));
    grid[(size_t)(3 * sx + 3)] = 1U;

    k3_scn_t s = {alive, loc_x, loc_y, desired_x, desired_y, lmd, grid};
    TEST_ASSERT_TRUE_MESSAGE(run_k3(&s), "K3 kernel dispatch failed");

    TEST_ASSERT_EQUAL_INT32_MESSAGE(3, result_loc_x[0], "loc_x should be unchanged");
    TEST_ASSERT_EQUAL_INT32_MESSAGE(3, result_loc_y[0], "loc_y should be unchanged");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(
        2U, result_last_move_dir[0], "last_move_dir should be unchanged"
    );
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
        1U, result_grid[(size_t)(3 * sx + 3)], "grid cell should still hold agent"
    );
}

/* An agent that desires a cell already occupied by another agent must stay.
 * Agent 0 is at (3,3) desiring (4,3); agent 1 is at (4,3) not moving.
 * After K3: agent 0 is still at (3,3); (4,3) still contains agent 1. */
void test_k3_occupied_cell_blocked(void) {
    TEST_ASSERT_EQUAL_INT_MESSAGE(BIOSIM_OK, fixture_status, "fixture setup failed");

    int sx = (int)sim.size_x;

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

    k3_scn_t s = {alive, loc_x, loc_y, desired_x, desired_y, lmd, grid};
    TEST_ASSERT_TRUE_MESSAGE(run_k3(&s), "K3 kernel dispatch failed");

    TEST_ASSERT_EQUAL_INT32_MESSAGE(3, result_loc_x[0], "agent 0 should be blocked");
    TEST_ASSERT_EQUAL_INT32_MESSAGE(3, result_loc_y[0], "agent 0 should be blocked");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
        2U, result_grid[(size_t)(3 * sx + 4)], "occupied cell should still hold agent 1"
    );
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
        1U, result_grid[(size_t)(3 * sx + 3)], "agent 0 old cell should still hold agent 0"
    );
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
