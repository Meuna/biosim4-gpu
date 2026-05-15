#include <stdint.h>
#include <string.h>

#include "biosim/core/census.h"
#include "biosim/core/challenge_defs.h"
#include "biosim/core/log.h"
#include "biosim/sim-gpu/pipeline.h"
#include "gpu_test_utils.h"
#include "unity.h"

/* ── test fixture ───────────────────────────────────────────────────────── */

static biosim_status_t fixture_status;
static biosim_sim_t sim;
static biosim_gpu_runner_t runner;
static biosim_gpu_pipeline_t pipeline;

/* ── Unity setUp / tearDown ─────────────────────────────────────────────── */

void setUp(void) {
}
void tearDown(void) {
}

/* ── global fixture setup / teardown ────────────────────────────────────── */

static void fixture_setup(void) {
    biosim_log_init(&biosim_log_default_ctx);

    fixture_status = sim_test_make_32x32(&sim);
    if (fixture_status != BIOSIM_OK) {
        return;
    }

    sim.challenge.kind = BIOSIM_CHALLENGE_TOUCH_ANY_WALL;

    cl_uint n = 0U;
    if (clGetPlatformIDs(0U, NULL, &n) != CL_SUCCESS || n == 0U) {
        fixture_status = BIOSIM_ERR_OPENCL;
        return;
    }

    fixture_status = biosim_gpu_runner_create(0U, 0U, &runner);
    if (fixture_status != BIOSIM_OK) {
        return;
    }

    memset(&pipeline, 0, sizeof(pipeline));
    fixture_status = biosim_gpu_pipeline_create(&sim, &runner, NULL, &pipeline);
}

static void fixture_teardown(void) {
    biosim_gpu_pipeline_free(&pipeline);
    biosim_gpu_runner_free(&runner);
    biosim_sim_free(&sim);
}

/* ── tests ──────────────────────────────────────────────────────────────── */

/* Verify that biosim_gpu_pipeline_create succeeds and all kernel handles are
 * non-NULL. */
void test_pipeline_compiles_and_creates(void) {
    TEST_ASSERT_EQUAL_INT_MESSAGE(BIOSIM_OK, fixture_status, "fixture setup failed");

    TEST_ASSERT_NOT_NULL(pipeline.k1);
    TEST_ASSERT_NOT_NULL(pipeline.k2);
    TEST_ASSERT_NOT_NULL(pipeline.k3);
    TEST_ASSERT_NOT_NULL(pipeline.k4);
    TEST_ASSERT_NOT_NULL(pipeline.k5);
    TEST_ASSERT_NOT_NULL(pipeline.k1_program);
    TEST_ASSERT_NOT_NULL(pipeline.k2_program);
    TEST_ASSERT_NOT_NULL(pipeline.k3_program);
    TEST_ASSERT_NOT_NULL(pipeline.k4_program);
    TEST_ASSERT_NOT_NULL(pipeline.k5_program);
}

/* Run all steps_per_gen steps without sync, then sync to host.
 * Verifies: no crash, alive count unchanged (no kills with TOUCH_ANY_WALL),
 * all positions in-bounds.
 * NOTE: final positions are not compared against the CPU reference because K3
 * uses atomic CAS for conflict resolution whose outcome is implementation-defined
 * when multiple agents contest the same cell (GPU vs. CPU ordering diverges). */
void test_pipeline_step_loop_no_crash(void) {
    TEST_ASSERT_EQUAL_INT_MESSAGE(BIOSIM_OK, fixture_status, "fixture setup failed");

    uint32_t steps = sim.steps_per_gen;
    for (uint32_t s = 0U; s < steps; s++) {
        sim.step = s;
        biosim_status_t rc = biosim_gpu_pipeline_step(&pipeline, &sim);
        TEST_ASSERT_EQUAL_INT(BIOSIM_OK, rc);
    }

    biosim_status_t rc = biosim_gpu_pipeline_sync_to_host(&pipeline, &sim);
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, rc);

    uint32_t pop = sim.population;
    uint32_t alive_count = 0U;
    for (uint32_t i = 0U; i < pop; i++) {
        if (sim.agents.alive[i]) {
            alive_count++;
            TEST_ASSERT_TRUE(sim.agents.loc_x[i] >= 0);
            TEST_ASSERT_TRUE(sim.agents.loc_x[i] < sim.size_x);
            TEST_ASSERT_TRUE(sim.agents.loc_y[i] >= 0);
            TEST_ASSERT_TRUE(sim.agents.loc_y[i] < sim.size_y);
        }
    }

    /* No kills: TOUCH_ANY_WALL + enable_kill=false → all agents still alive. */
    TEST_ASSERT_EQUAL_UINT32(pop, alive_count);
}

/* Run a full generation (step loop + sync_to_host + next_generation +
 * sync_from_host) then run a second step loop.  Verifies the upload/download
 * cycle and that the pipeline continues to work after a generation boundary. */
void test_pipeline_generation_boundary(void) {
    TEST_ASSERT_EQUAL_INT_MESSAGE(BIOSIM_OK, fixture_status, "fixture setup failed");

    /* ── generation 0 step loop ─────────────────────────────────────────── */
    uint32_t steps = sim.steps_per_gen;
    for (uint32_t s = 0U; s < steps; s++) {
        sim.step = s;
        TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_gpu_pipeline_step(&pipeline, &sim));
    }

    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_gpu_pipeline_sync_to_host(&pipeline, &sim));

    /* ── generation boundary ─────────────────────────────────────────────── */
    biosim_census_t census;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_sim_next_generation(&sim, &census));

    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_gpu_pipeline_sync_from_host(&pipeline, &sim));

    /* ── generation 1 step loop ─────────────────────────────────────────── */
    for (uint32_t s = 0U; s < steps; s++) {
        sim.step = s;
        TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_gpu_pipeline_step(&pipeline, &sim));
    }

    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_gpu_pipeline_sync_to_host(&pipeline, &sim));

    /* All agents alive after gen 1 (TOUCH_ANY_WALL + no kills). */
    uint32_t pop = sim.population;
    uint32_t alive_count = 0U;
    for (uint32_t i = 0U; i < pop; i++) {
        if (sim.agents.alive[i]) {
            alive_count++;
        }
    }
    TEST_ASSERT_EQUAL_UINT32(pop, alive_count);
}

/* ── main ───────────────────────────────────────────────────────────────── */

int main(void) {
    UNITY_BEGIN();
    fixture_setup();
    RUN_TEST(test_pipeline_compiles_and_creates);
    RUN_TEST(test_pipeline_step_loop_no_crash);
    RUN_TEST(test_pipeline_generation_boundary);
    fixture_teardown();
    return UNITY_END();
}
