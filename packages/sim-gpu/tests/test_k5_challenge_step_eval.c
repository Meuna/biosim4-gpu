#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/opencl.h>
#endif

#include "biosim/core/challenge_kinds.h"
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

static cl_mem g_buf_alive;
static cl_mem g_buf_loc_x;
static cl_mem g_buf_loc_y;
static cl_mem g_buf_challenge_bits;
static cl_mem g_buf_rng_state;
static cl_mem g_buf_grid;
static cl_mem g_buf_barrier_ctrs;

static uint8_t *g_result_alive;
static uint32_t *g_result_cbits;
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
    if (biosim_gpu_registry_get("k5_challenge_step_eval", NULL, &sources) != BIOSIM_OK) {
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
    g_kernel = clCreateKernel(g_program, "k_challenge_step_eval", &cl_err);
    if (cl_err != CL_SUCCESS || !g_kernel) {
        g_opencl_ok = 0;
        return;
    }

    uint32_t pop = g_sim.population;
    size_t grid_cells = (size_t)g_sim.size_x * (size_t)g_sim.size_y;

    g_result_alive = malloc((size_t)pop * sizeof(uint8_t));
    g_result_cbits = malloc((size_t)pop * sizeof(uint32_t));
    g_result_grid = malloc(grid_cells * sizeof(uint32_t));
    if (!g_result_alive || !g_result_cbits || !g_result_grid) {
        g_opencl_ok = 0;
        return;
    }

    /* Allocate device buffers at maximum size; contents are uploaded per test. */
    g_buf_alive = clCreateBuffer(g_runner.context, CL_MEM_READ_WRITE,
                                 (size_t)pop * sizeof(uint8_t), NULL, &cl_err);
    g_buf_loc_x = clCreateBuffer(g_runner.context, CL_MEM_READ_WRITE,
                                 (size_t)pop * sizeof(int32_t), NULL, &cl_err);
    g_buf_loc_y = clCreateBuffer(g_runner.context, CL_MEM_READ_WRITE,
                                 (size_t)pop * sizeof(int32_t), NULL, &cl_err);
    g_buf_challenge_bits = clCreateBuffer(g_runner.context, CL_MEM_READ_WRITE,
                                          (size_t)pop * sizeof(uint32_t), NULL, &cl_err);
    g_buf_rng_state = clCreateBuffer(g_runner.context, CL_MEM_READ_WRITE,
                                     (size_t)pop * sizeof(uint64_t), NULL, &cl_err);
    g_buf_grid = clCreateBuffer(g_runner.context, CL_MEM_READ_WRITE,
                                grid_cells * sizeof(uint32_t), NULL, &cl_err);
    /* barrier_ctrs: 2 ints minimum (even when unused) */
    g_buf_barrier_ctrs = clCreateBuffer(g_runner.context, CL_MEM_READ_ONLY,
                                        2U * sizeof(int32_t), NULL, &cl_err);

    if (!g_buf_alive || !g_buf_loc_x || !g_buf_loc_y || !g_buf_challenge_bits ||
        !g_buf_rng_state || !g_buf_grid || !g_buf_barrier_ctrs) {
        g_opencl_ok = 0;
    }
}

static void fixture_teardown(void) {
    free(g_result_alive);
    free(g_result_cbits);
    free(g_result_grid);
    g_result_alive = NULL;
    g_result_cbits = NULL;
    g_result_grid = NULL;

    if (g_buf_barrier_ctrs) {
        clReleaseMemObject(g_buf_barrier_ctrs);
        g_buf_barrier_ctrs = NULL;
    }
    if (g_buf_grid) {
        clReleaseMemObject(g_buf_grid);
        g_buf_grid = NULL;
    }
    if (g_buf_rng_state) {
        clReleaseMemObject(g_buf_rng_state);
        g_buf_rng_state = NULL;
    }
    if (g_buf_challenge_bits) {
        clReleaseMemObject(g_buf_challenge_bits);
        g_buf_challenge_bits = NULL;
    }
    if (g_buf_loc_y) {
        clReleaseMemObject(g_buf_loc_y);
        g_buf_loc_y = NULL;
    }
    if (g_buf_loc_x) {
        clReleaseMemObject(g_buf_loc_x);
        g_buf_loc_x = NULL;
    }
    if (g_buf_alive) {
        clReleaseMemObject(g_buf_alive);
        g_buf_alive = NULL;
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
} k5_scenario_t;

static int run_k5(const k5_scenario_t *s) {
    cl_command_queue q = g_runner.queue;
    uint32_t pop = g_sim.population;
    size_t grid_cells = (size_t)g_sim.size_x * (size_t)g_sim.size_y;

    /* Upload per-agent arrays */
    if (clEnqueueWriteBuffer(q, g_buf_alive, CL_FALSE, 0U, (size_t)pop * sizeof(uint8_t),
                             s->alive, 0U, NULL, NULL) != CL_SUCCESS) {
        return 0;
    }
    if (clEnqueueWriteBuffer(q, g_buf_loc_x, CL_FALSE, 0U, (size_t)pop * sizeof(int32_t), s->loc_x,
                             0U, NULL, NULL) != CL_SUCCESS) {
        return 0;
    }
    if (clEnqueueWriteBuffer(q, g_buf_loc_y, CL_FALSE, 0U, (size_t)pop * sizeof(int32_t), s->loc_y,
                             0U, NULL, NULL) != CL_SUCCESS) {
        return 0;
    }
    if (clEnqueueWriteBuffer(q, g_buf_challenge_bits, CL_FALSE, 0U,
                             (size_t)pop * sizeof(uint32_t), s->cbits, 0U, NULL,
                             NULL) != CL_SUCCESS) {
        return 0;
    }
    if (clEnqueueWriteBuffer(q, g_buf_rng_state, CL_FALSE, 0U, (size_t)pop * sizeof(uint64_t),
                             s->rng, 0U, NULL, NULL) != CL_SUCCESS) {
        return 0;
    }
    if (clEnqueueWriteBuffer(q, g_buf_grid, CL_FALSE, 0U, grid_cells * sizeof(uint32_t), s->grid,
                             0U, NULL, NULL) != CL_SUCCESS) {
        return 0;
    }
    /* barrier_ctrs: upload 2 ints minimum */
    size_t ctr_bytes = (s->n_barrier_ctrs == 0U) ? 2U * sizeof(int32_t)
                                                  : (size_t)s->n_barrier_ctrs * 2U * sizeof(int32_t);
    if (clEnqueueWriteBuffer(q, g_buf_barrier_ctrs, CL_FALSE, 0U, ctr_bytes, s->barrier_ctrs, 0U,
                             NULL, NULL) != CL_SUCCESS) {
        return 0;
    }

    /* Set kernel args */
    cl_int size_x = g_sim.size_x;
    cl_int size_y = g_sim.size_y;
    cl_uint step = s->step;
    cl_uint steps_gen = s->steps_per_gen;
    cl_uint kind = s->challenge_kind;
    cl_float radius = s->radius;
    cl_uint n_ctrs = s->n_barrier_ctrs;

    (void)clSetKernelArg(g_kernel, 0U, sizeof(cl_mem), (const void *)&g_buf_alive);
    (void)clSetKernelArg(g_kernel, 1U, sizeof(cl_mem), (const void *)&g_buf_loc_x);
    (void)clSetKernelArg(g_kernel, 2U, sizeof(cl_mem), (const void *)&g_buf_loc_y);
    (void)clSetKernelArg(g_kernel, 3U, sizeof(cl_mem), (const void *)&g_buf_challenge_bits);
    (void)clSetKernelArg(g_kernel, 4U, sizeof(cl_mem), (const void *)&g_buf_rng_state);
    (void)clSetKernelArg(g_kernel, 5U, sizeof(cl_mem), (const void *)&g_buf_grid);
    (void)clSetKernelArg(g_kernel, 6U, sizeof(cl_int), (const void *)&size_x);
    (void)clSetKernelArg(g_kernel, 7U, sizeof(cl_int), (const void *)&size_y);
    (void)clSetKernelArg(g_kernel, 8U, sizeof(cl_uint), (const void *)&step);
    (void)clSetKernelArg(g_kernel, 9U, sizeof(cl_uint), (const void *)&steps_gen);
    (void)clSetKernelArg(g_kernel, 10U, sizeof(cl_uint), (const void *)&kind);
    (void)clSetKernelArg(g_kernel, 11U, sizeof(cl_float), (const void *)&radius);
    (void)clSetKernelArg(g_kernel, 12U, sizeof(cl_mem), (const void *)&g_buf_barrier_ctrs);
    (void)clSetKernelArg(g_kernel, 13U, sizeof(cl_uint), (const void *)&n_ctrs);

    size_t global_size = (size_t)pop;
    if (clEnqueueNDRangeKernel(q, g_kernel, 1U, NULL, &global_size, NULL, 0U, NULL, NULL) !=
        CL_SUCCESS) {
        return 0;
    }

    /* Readback alive, challenge_bits, grid */
    if (clEnqueueReadBuffer(q, g_buf_alive, CL_FALSE, 0U, (size_t)pop * sizeof(uint8_t),
                            g_result_alive, 0U, NULL, NULL) != CL_SUCCESS) {
        return 0;
    }
    if (clEnqueueReadBuffer(q, g_buf_challenge_bits, CL_FALSE, 0U,
                            (size_t)pop * sizeof(uint32_t), g_result_cbits, 0U, NULL,
                            NULL) != CL_SUCCESS) {
        return 0;
    }
    if (clEnqueueReadBuffer(q, g_buf_grid, CL_TRUE, 0U, grid_cells * sizeof(uint32_t),
                            g_result_grid, 0U, NULL, NULL) != CL_SUCCESS) {
        return 0;
    }
    return 1;
}

/* ── tests ──────────────────────────────────────────────────────────────── */

/* Unsupported challenge kind (X_BAND) — kernel must be a no-op. */
void test_k5_compiles_and_runs_noop(void) {
    if (!g_opencl_ok) {
        TEST_IGNORE_MESSAGE("OpenCL not available");
    }

    uint32_t pop = g_sim.population;
    size_t grid_cells = (size_t)g_sim.size_x * (size_t)g_sim.size_y;

    uint8_t *alive = calloc(pop, sizeof(uint8_t));
    int32_t *loc_x = calloc(pop, sizeof(int32_t));
    int32_t *loc_y = calloc(pop, sizeof(int32_t));
    uint32_t *cbits = calloc(pop, sizeof(uint32_t));
    uint64_t *rng = calloc(pop, sizeof(uint64_t));
    uint32_t *grid = calloc(grid_cells, sizeof(uint32_t));
    int32_t dummy_ctr[2] = {0, 0};
    TEST_ASSERT_NOT_NULL(alive);
    TEST_ASSERT_NOT_NULL(loc_x);
    TEST_ASSERT_NOT_NULL(loc_y);
    TEST_ASSERT_NOT_NULL(cbits);
    TEST_ASSERT_NOT_NULL(rng);
    TEST_ASSERT_NOT_NULL(grid);

    alive[0] = 1U;
    loc_x[0] = 3;
    loc_y[0] = 3;
    rng[0] = 12345U;
    grid[(size_t)3 * (size_t)g_sim.size_x + 3U] = 1U;

    k5_scenario_t s = {alive,          loc_x,
                       loc_y,          cbits,
                       rng,            grid,
                       dummy_ctr,      0U,
                       0U,             g_sim.steps_per_gen,
                       BIOSIM_CHALLENGE_X_BAND, 0.0F};

    TEST_ASSERT_TRUE_MESSAGE(run_k5(&s), "K5 kernel dispatch failed");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1U, g_result_alive[0], "agent must stay alive for no-op kind");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0U, g_result_cbits[0], "challenge_bits must not change");

    free(alive);
    free(loc_x);
    free(loc_y);
    free(cbits);
    free(rng);
    free(grid);
}

/* TOUCH_ANY_WALL: agent at border sets challenge_bits; agent in middle does not. */
void test_k5_touch_any_wall_sets_bit(void) {
    if (!g_opencl_ok) {
        TEST_IGNORE_MESSAGE("OpenCL not available");
    }

    uint32_t pop = g_sim.population;
    size_t grid_cells = (size_t)g_sim.size_x * (size_t)g_sim.size_y;

    uint8_t *alive = calloc(pop, sizeof(uint8_t));
    int32_t *loc_x = calloc(pop, sizeof(int32_t));
    int32_t *loc_y = calloc(pop, sizeof(int32_t));
    uint32_t *cbits = calloc(pop, sizeof(uint32_t));
    uint64_t *rng = calloc(pop, sizeof(uint64_t));
    uint32_t *grid = calloc(grid_cells, sizeof(uint32_t));
    int32_t dummy_ctr[2] = {0, 0};
    TEST_ASSERT_NOT_NULL(alive);
    TEST_ASSERT_NOT_NULL(loc_x);
    TEST_ASSERT_NOT_NULL(loc_y);
    TEST_ASSERT_NOT_NULL(cbits);
    TEST_ASSERT_NOT_NULL(rng);
    TEST_ASSERT_NOT_NULL(grid);

    /* Agent 0: at border (0,0) */
    alive[0] = 1U;
    loc_x[0] = 0;
    loc_y[0] = 0;
    grid[0] = 1U;

    /* Agent 1: in middle */
    alive[1] = 1U;
    loc_x[1] = 3;
    loc_y[1] = 3;
    grid[(size_t)3 * (size_t)g_sim.size_x + 3U] = 2U;

    k5_scenario_t s = {alive,
                       loc_x,
                       loc_y,
                       cbits,
                       rng,
                       grid,
                       dummy_ctr,
                       0U,
                       0U,
                       g_sim.steps_per_gen,
                       BIOSIM_CHALLENGE_TOUCH_ANY_WALL,
                       0.0F};

    TEST_ASSERT_TRUE_MESSAGE(run_k5(&s), "K5 kernel dispatch failed");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1U, g_result_cbits[0], "border agent must have bit set");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0U, g_result_cbits[1], "interior agent must have no bit set");
    /* alive unchanged for this challenge */
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1U, g_result_alive[0], "touch_any_wall must not kill");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1U, g_result_alive[1], "touch_any_wall must not kill");

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
    if (!g_opencl_ok) {
        TEST_IGNORE_MESSAGE("OpenCL not available");
    }

    uint32_t pop = g_sim.population;
    size_t grid_cells = (size_t)g_sim.size_x * (size_t)g_sim.size_y;

    uint8_t *alive = calloc(pop, sizeof(uint8_t));
    int32_t *loc_x = calloc(pop, sizeof(int32_t));
    int32_t *loc_y = calloc(pop, sizeof(int32_t));
    uint32_t *cbits = calloc(pop, sizeof(uint32_t));
    uint64_t *rng = calloc(pop, sizeof(uint64_t));
    uint32_t *grid = calloc(grid_cells, sizeof(uint32_t));
    int32_t dummy_ctr[2] = {0, 0};
    TEST_ASSERT_NOT_NULL(alive);
    TEST_ASSERT_NOT_NULL(loc_x);
    TEST_ASSERT_NOT_NULL(loc_y);
    TEST_ASSERT_NOT_NULL(cbits);
    TEST_ASSERT_NOT_NULL(rng);
    TEST_ASSERT_NOT_NULL(grid);

    /* step=0 < steps_per_gen/2=50 → radioactive_x = 0; agent at x=0 → dist=0 */
    alive[0] = 1U;
    loc_x[0] = 0;
    loc_y[0] = 2;
    rng[0] = 9999U;
    grid[(size_t)2 * (size_t)g_sim.size_x + 0U] = 1U;

    k5_scenario_t s = {alive,
                       loc_x,
                       loc_y,
                       cbits,
                       rng,
                       grid,
                       dummy_ctr,
                       0U,
                       0U,
                       g_sim.steps_per_gen,
                       BIOSIM_CHALLENGE_RADIOACTIVE_WALLS,
                       0.0F};

    TEST_ASSERT_TRUE_MESSAGE(run_k5(&s), "K5 kernel dispatch failed");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0U, g_result_alive[0], "agent at dist==0 must be killed");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0U, g_result_grid[(size_t)2 * (size_t)g_sim.size_x + 0U],
                                     "grid cell of killed agent must be cleared");

    free(alive);
    free(loc_x);
    free(loc_y);
    free(cbits);
    free(rng);
    free(grid);
}

/* LOCATION_SEQUENCE: agent at barrier centre acquires bit 0. */
void test_k5_location_sequence_sets_bit(void) {
    if (!g_opencl_ok) {
        TEST_IGNORE_MESSAGE("OpenCL not available");
    }

    uint32_t pop = g_sim.population;
    size_t grid_cells = (size_t)g_sim.size_x * (size_t)g_sim.size_y;

    uint8_t *alive = calloc(pop, sizeof(uint8_t));
    int32_t *loc_x = calloc(pop, sizeof(int32_t));
    int32_t *loc_y = calloc(pop, sizeof(int32_t));
    uint32_t *cbits = calloc(pop, sizeof(uint32_t));
    uint64_t *rng = calloc(pop, sizeof(uint64_t));
    uint32_t *grid = calloc(grid_cells, sizeof(uint32_t));
    int32_t barrier_ctr[2] = {4, 4};
    TEST_ASSERT_NOT_NULL(alive);
    TEST_ASSERT_NOT_NULL(loc_x);
    TEST_ASSERT_NOT_NULL(loc_y);
    TEST_ASSERT_NOT_NULL(cbits);
    TEST_ASSERT_NOT_NULL(rng);
    TEST_ASSERT_NOT_NULL(grid);

    /* Agent 0 at barrier centre (4,4); radius=1.0 → rpx=size_x → any dist qualifies */
    alive[0] = 1U;
    loc_x[0] = 4;
    loc_y[0] = 4;
    grid[(size_t)4 * (size_t)g_sim.size_x + 4U] = 1U;

    k5_scenario_t s = {alive,
                       loc_x,
                       loc_y,
                       cbits,
                       rng,
                       grid,
                       barrier_ctr,
                       1U,
                       0U,
                       g_sim.steps_per_gen,
                       BIOSIM_CHALLENGE_LOCATION_SEQUENCE,
                       1.0F};

    TEST_ASSERT_TRUE_MESSAGE(run_k5(&s), "K5 kernel dispatch failed");
    TEST_ASSERT_TRUE_MESSAGE((g_result_cbits[0] & 1U) != 0U,
                             "agent at barrier centre must acquire bit 0");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1U, g_result_alive[0],
                                    "location_sequence must not kill agent");

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
    RUN_TEST(test_k5_compiles_and_runs_noop);
    RUN_TEST(test_k5_touch_any_wall_sets_bit);
    RUN_TEST(test_k5_radioactive_walls_kills_at_wall);
    RUN_TEST(test_k5_location_sequence_sets_bit);
    fixture_teardown();
    return UNITY_END();
}
