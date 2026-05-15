#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "biosim/core/challenge_defs.h"
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

static cl_mem buf_alive;
static cl_mem buf_loc_x;
static cl_mem buf_loc_y;
static cl_mem buf_challenge_bits;
static cl_mem buf_rng_state;
static cl_mem buf_grid;
static cl_mem buf_barrier_ctrs;

static uint8_t *result_alive;
static uint32_t *result_cbits;
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
    sim.steps_per_gen = 100U;

    fixture_status = gpu_test_kernel_runtime_create(
        &runner, &program, &kernel, "k5_challenge_step_eval", "k_challenge_step_eval"
    );
    if (fixture_status != BIOSIM_OK) {
        return;
    }

    uint32_t pop = sim.population;
    size_t grid_size = (size_t)sim.size_x * (size_t)sim.size_y;

    /* Allocate host-side readback and snapshot buffers. */
    ALLOC(result_alive, pop, sizeof(uint8_t));
    ALLOC(result_cbits, pop, sizeof(uint32_t));
    ALLOC(result_grid, grid_size, sizeof(uint32_t));

    /* Allocate device buffers at maximum size; contents are uploaded per test. */
    MKRW(buf_alive, NULL, sizeof(uint8_t), pop);
    MKRW(buf_loc_x, NULL, sizeof(int32_t), pop);
    MKRW(buf_loc_y, NULL, sizeof(int32_t), pop);
    MKRW(buf_challenge_bits, NULL, sizeof(uint32_t), pop);
    MKRW(buf_rng_state, NULL, sizeof(uint64_t), pop);
    MKRW(buf_grid, NULL, sizeof(uint32_t), grid_size);
    /* barrier_ctrs: 2 ints minimum (even when unused) */
    MKRO(buf_barrier_ctrs, NULL, sizeof(int32_t), 2U);
}

static void fixture_teardown(void) {
    free(result_alive);
    free(result_cbits);
    free(result_grid);

    SAFE_RELEASE(clReleaseMemObject, buf_barrier_ctrs);
    SAFE_RELEASE(clReleaseMemObject, buf_grid);
    SAFE_RELEASE(clReleaseMemObject, buf_rng_state);
    SAFE_RELEASE(clReleaseMemObject, buf_challenge_bits);
    SAFE_RELEASE(clReleaseMemObject, buf_loc_y);
    SAFE_RELEASE(clReleaseMemObject, buf_loc_x);
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
    const uint32_t *cbits;
    const uint64_t *rng;
    const uint32_t *grid;
    const int32_t *barrier_ctrs;
    uint32_t n_barrier_ctrs;
    uint32_t step;
    uint32_t steps_per_gen;
    uint32_t challenge_kind;
    float radius;
} k5_scn_t;

static int run_k5(const k5_scn_t *s) {
    /* Set kernel args */
    cl_int size_x = sim.size_x;
    cl_int size_y = sim.size_y;
    cl_uint step = s->step;
    cl_uint steps_gen = s->steps_per_gen;
    cl_uint kind = s->challenge_kind;
    cl_float radius = s->radius;
    cl_uint n_ctrs = s->n_barrier_ctrs;

    (void)clSetKernelArg(kernel, 0U, sizeof(cl_mem), (const void *)&buf_alive);
    (void)clSetKernelArg(kernel, 1U, sizeof(cl_mem), (const void *)&buf_loc_x);
    (void)clSetKernelArg(kernel, 2U, sizeof(cl_mem), (const void *)&buf_loc_y);
    (void)clSetKernelArg(kernel, 3U, sizeof(cl_mem), (const void *)&buf_challenge_bits);
    (void)clSetKernelArg(kernel, 4U, sizeof(cl_mem), (const void *)&buf_rng_state);
    (void)clSetKernelArg(kernel, 5U, sizeof(cl_mem), (const void *)&buf_grid);
    (void)clSetKernelArg(kernel, 6U, sizeof(cl_int), (const void *)&size_x);
    (void)clSetKernelArg(kernel, 7U, sizeof(cl_int), (const void *)&size_y);
    (void)clSetKernelArg(kernel, 8U, sizeof(cl_uint), (const void *)&step);
    (void)clSetKernelArg(kernel, 9U, sizeof(cl_uint), (const void *)&steps_gen);
    (void)clSetKernelArg(kernel, 10U, sizeof(cl_uint), (const void *)&kind);
    (void)clSetKernelArg(kernel, 11U, sizeof(cl_float), (const void *)&radius);
    (void)clSetKernelArg(kernel, 12U, sizeof(cl_mem), (const void *)&buf_barrier_ctrs);
    (void)clSetKernelArg(kernel, 13U, sizeof(cl_uint), (const void *)&n_ctrs);

    cl_command_queue q = runner.queue;
    uint32_t pop = sim.population;
    size_t grid_size = (size_t)sim.size_x * (size_t)sim.size_y;

    /* Enqueue kernel run */
    WRITE(buf_alive, s->alive, pop, sizeof(uint8_t));
    WRITE(buf_loc_x, s->loc_x, pop, sizeof(int32_t));
    WRITE(buf_loc_y, s->loc_y, pop, sizeof(int32_t));
    WRITE(buf_challenge_bits, s->cbits, pop, sizeof(uint32_t));
    WRITE(buf_rng_state, s->rng, pop, sizeof(uint64_t));
    WRITE(buf_grid, s->grid, grid_size, sizeof(uint32_t));
    /* barrier_ctrs: upload 2 ints minimum */
    size_t ctr_bytes = (s->n_barrier_ctrs == 0U) ? 2U * sizeof(int32_t)
                                                 : (size_t)s->n_barrier_ctrs * 2U * sizeof(int32_t);
    WRITE(buf_barrier_ctrs, s->barrier_ctrs, 1U, ctr_bytes);

    size_t global_size = (size_t)pop;
    if (clEnqueueNDRangeKernel(q, kernel, 1U, NULL, &global_size, NULL, 0U, NULL, NULL) !=
        CL_SUCCESS) {
        return 0;
    }

    READ(buf_alive, result_alive, pop, sizeof(uint8_t));
    READ(buf_challenge_bits, result_cbits, pop, sizeof(uint32_t));
    READ(buf_grid, result_grid, grid_size, sizeof(uint32_t));

    /* Wait for the queue to complete */
    if (clFinish(q) != CL_SUCCESS) {
        return 0;
    }

    return 1;
}

/* ── tests ──────────────────────────────────────────────────────────────── */

/* TOUCH_ANY_WALL: agent at border sets challenge_bits; agent in middle does not. */
void test_k5_touch_any_wall_sets_bit(void) {
    TEST_ASSERT_EQUAL_INT_MESSAGE(BIOSIM_OK, fixture_status, "fixture setup failed");

    uint32_t pop = sim.population;
    size_t grid_size = (size_t)sim.size_x * (size_t)sim.size_y;

    uint8_t *alive = calloc_test_assert(pop, sizeof(uint8_t));
    int32_t *loc_x = calloc_test_assert(pop, sizeof(int32_t));
    int32_t *loc_y = calloc_test_assert(pop, sizeof(int32_t));
    uint32_t *cbits = calloc_test_assert(pop, sizeof(uint32_t));
    uint64_t *rng = calloc_test_assert(pop, sizeof(uint64_t));
    uint32_t *grid = calloc_test_assert(grid_size, sizeof(uint32_t));
    int32_t dummy_ctr[2] = {0, 0};

    /* Agent 0: at border (0,0) */
    alive[0] = 1U;
    loc_x[0] = 0;
    loc_y[0] = 0;
    grid[0] = 1U;

    /* Agent 1: in middle */
    alive[1] = 1U;
    loc_x[1] = 3;
    loc_y[1] = 3;
    grid[(size_t)3 * (size_t)sim.size_x + 3U] = 2U;

    k5_scn_t s = {
        alive,
        loc_x,
        loc_y,
        cbits,
        rng,
        grid,
        dummy_ctr,
        0U,
        0U,
        sim.steps_per_gen,
        BIOSIM_CHALLENGE_TOUCH_ANY_WALL,
        0.0F
    };

    TEST_ASSERT_TRUE_MESSAGE(run_k5(&s), "K5 kernel dispatch failed");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1U, result_cbits[0], "border agent must have bit set");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0U, result_cbits[1], "interior agent must have no bit set");
    /* alive unchanged for this challenge */
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1U, result_alive[0], "touch_any_wall must not kill");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1U, result_alive[1], "touch_any_wall must not kill");

    free(alive);
    free(loc_x);
    free(loc_y);
    free(cbits);
    free(rng);
    free(grid);
}

/* RADIOACTIVE_WALLS: agent at dist==0 from radioactive wall is killed;
 * its grid cell is cleared. */
void test_k5_radioactive_walls_kills_at_wall(void) {
    TEST_ASSERT_EQUAL_INT_MESSAGE(BIOSIM_OK, fixture_status, "fixture setup failed");

    uint32_t pop = sim.population;
    size_t grid_size = (size_t)sim.size_x * (size_t)sim.size_y;

    uint8_t *alive = calloc_test_assert(pop, sizeof(uint8_t));
    int32_t *loc_x = calloc_test_assert(pop, sizeof(int32_t));
    int32_t *loc_y = calloc_test_assert(pop, sizeof(int32_t));
    uint32_t *cbits = calloc_test_assert(pop, sizeof(uint32_t));
    uint64_t *rng = calloc_test_assert(pop, sizeof(uint64_t));
    uint32_t *grid = calloc_test_assert(grid_size, sizeof(uint32_t));
    int32_t dummy_ctr[2] = {0, 0};

    /* step=0 < steps_per_gen/2=50 → radioactive_x = 0; agent at x=0 → dist=0 */
    alive[0] = 1U;
    loc_x[0] = 0;
    loc_y[0] = 2;
    rng[0] = 9999U;
    grid[(size_t)2 * (size_t)sim.size_x + 0U] = 1U;

    k5_scn_t s = {
        alive,
        loc_x,
        loc_y,
        cbits,
        rng,
        grid,
        dummy_ctr,
        0U,
        0U,
        sim.steps_per_gen,
        BIOSIM_CHALLENGE_RADIOACTIVE_WALLS,
        0.0F
    };

    TEST_ASSERT_TRUE_MESSAGE(run_k5(&s), "K5 kernel dispatch failed");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0U, result_alive[0], "agent at dist==0 must be killed");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
        0U,
        result_grid[(size_t)2 * (size_t)sim.size_x + 0U],
        "grid cell of killed agent must be cleared"
    );

    free(alive);
    free(loc_x);
    free(loc_y);
    free(cbits);
    free(rng);
    free(grid);
}

/* LOCATION_SEQUENCE: agent at barrier centre acquires bit 0. */
void test_k5_location_sequence_sets_bit(void) {
    TEST_ASSERT_EQUAL_INT_MESSAGE(BIOSIM_OK, fixture_status, "fixture setup failed");

    uint32_t pop = sim.population;
    size_t grid_size = (size_t)sim.size_x * (size_t)sim.size_y;

    uint8_t *alive = calloc_test_assert(pop, sizeof(uint8_t));
    int32_t *loc_x = calloc_test_assert(pop, sizeof(int32_t));
    int32_t *loc_y = calloc_test_assert(pop, sizeof(int32_t));
    uint32_t *cbits = calloc_test_assert(pop, sizeof(uint32_t));
    uint64_t *rng = calloc_test_assert(pop, sizeof(uint64_t));
    uint32_t *grid = calloc_test_assert(grid_size, sizeof(uint32_t));
    int32_t barrier_ctr[2] = {4, 4};

    /* Agent 0 at barrier centre (4,4); radius=1.0 → rpx=size_x → any dist qualifies */
    alive[0] = 1U;
    loc_x[0] = 4;
    loc_y[0] = 4;
    grid[(size_t)4 * (size_t)sim.size_x + 4U] = 1U;

    k5_scn_t s = {
        alive,
        loc_x,
        loc_y,
        cbits,
        rng,
        grid,
        barrier_ctr,
        1U,
        0U,
        sim.steps_per_gen,
        BIOSIM_CHALLENGE_LOCATION_SEQUENCE,
        1.0F
    };

    TEST_ASSERT_TRUE_MESSAGE(run_k5(&s), "K5 kernel dispatch failed");
    TEST_ASSERT_TRUE_MESSAGE(
        (result_cbits[0] & 1U) != 0U, "agent at barrier centre must acquire bit 0"
    );
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1U, result_alive[0], "location_sequence must not kill agent");

    free(alive);
    free(loc_x);
    free(loc_y);
    free(cbits);
    free(rng);
    free(grid);
}

/* ── main ───────────────────────────────────────────────────────────────── */

int main(void) {
    UNITY_BEGIN();
    fixture_setup();
    RUN_TEST(test_k5_touch_any_wall_sets_bit);
    RUN_TEST(test_k5_radioactive_walls_kills_at_wall);
    RUN_TEST(test_k5_location_sequence_sets_bit);
    fixture_teardown();
    return UNITY_END();
}
