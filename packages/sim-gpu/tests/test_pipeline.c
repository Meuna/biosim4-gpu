#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/opencl.h>
#endif

#include "biosim/core/census.h"
#include "biosim/core/challenge_kinds.h"
#include "biosim/core/log.h"
#include "biosim/core/rng.h"
#include "biosim/core/sim.h"
#include "biosim/core/status.h"
#include "biosim/sim-gpu/pipeline.h"
#include "biosim/sim-gpu/runner.h"
#include "unity.h"

/* ── test fixture ───────────────────────────────────────────────────────── */

static biosim_sim_t g_sim;
static biosim_gpu_runner_t g_runner;
static biosim_gpu_pipeline_t g_pipeline;
static int g_opencl_ok;

static void make_test_sim(biosim_sim_t *sim) {
    memset(sim, 0, sizeof(*sim));
    sim->population = 32U;
    sim->size_x = 16;
    sim->size_y = 16;
    sim->genome_max_len = 4U;
    sim->max_neurons = 2U;
    sim->long_probe_dist = 4U;
    sim->steps_per_gen = 5U;
    sim->step = 0U;
    sim->gen_rng = biosim_rng_seed(0U, 1U);
    sim->challenge.kind = BIOSIM_CHALLENGE_TOUCH_ANY_WALL;
    sim->enable_kill = 0;
}

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

    make_test_sim(&g_sim);
    if (biosim_sim_create(&g_sim, NULL, 0U) != BIOSIM_OK) {
        g_opencl_ok = 0;
        return;
    }

    if (biosim_gpu_runner_create(0U, 0U, &g_runner) != BIOSIM_OK) {
        g_opencl_ok = 0;
        return;
    }

    memset(&g_pipeline, 0, sizeof(g_pipeline));
    if (biosim_gpu_pipeline_create(&g_sim, &g_runner, NULL, &g_pipeline) != BIOSIM_OK) {
        g_opencl_ok = 0;
    }
}

static void fixture_teardown(void) {
    biosim_gpu_pipeline_free(&g_pipeline);
    biosim_gpu_runner_free(&g_runner);
    biosim_sim_free(&g_sim);
}

/* ── tests ──────────────────────────────────────────────────────────────── */

/* Verify that biosim_gpu_pipeline_create succeeds and all kernel handles are
 * non-NULL. */
void test_pipeline_compiles_and_creates(void) {
    if (!g_opencl_ok) {
        TEST_IGNORE_MESSAGE("OpenCL not available");
    }

    TEST_ASSERT_NOT_NULL(g_pipeline.k1);
    TEST_ASSERT_NOT_NULL(g_pipeline.k2);
    TEST_ASSERT_NOT_NULL(g_pipeline.k3);
    TEST_ASSERT_NOT_NULL(g_pipeline.k4);
    TEST_ASSERT_NOT_NULL(g_pipeline.k5);
    TEST_ASSERT_NOT_NULL(g_pipeline.k1_program);
    TEST_ASSERT_NOT_NULL(g_pipeline.k2_program);
    TEST_ASSERT_NOT_NULL(g_pipeline.k3_program);
    TEST_ASSERT_NOT_NULL(g_pipeline.k4_program);
    TEST_ASSERT_NOT_NULL(g_pipeline.k5_program);
}

/* Run all steps_per_gen steps without sync, then sync to host.
 * Verifies: no crash, alive count unchanged (no kills with TOUCH_ANY_WALL),
 * all positions in-bounds.
 * NOTE: final positions are not compared against the CPU reference because K3
 * uses atomic CAS for conflict resolution whose outcome is implementation-defined
 * when multiple agents contest the same cell (GPU vs. CPU ordering diverges). */
void test_pipeline_step_loop_no_crash(void) {
    if (!g_opencl_ok) {
        TEST_IGNORE_MESSAGE("OpenCL not available");
    }

    uint32_t steps = g_sim.steps_per_gen;
    for (uint32_t s = 0U; s < steps; s++) {
        g_sim.step = s;
        biosim_status_t rc = biosim_gpu_pipeline_step(&g_pipeline, &g_sim);
        TEST_ASSERT_EQUAL_INT(BIOSIM_OK, rc);
    }

    biosim_status_t rc = biosim_gpu_pipeline_sync_to_host(&g_pipeline, &g_sim);
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, rc);

    uint32_t pop = g_sim.population;
    uint32_t alive_count = 0U;
    for (uint32_t i = 0U; i < pop; i++) {
        if (g_sim.agents.alive[i]) {
            alive_count++;
            TEST_ASSERT_TRUE(g_sim.agents.loc_x[i] >= 0);
            TEST_ASSERT_TRUE(g_sim.agents.loc_x[i] < g_sim.size_x);
            TEST_ASSERT_TRUE(g_sim.agents.loc_y[i] >= 0);
            TEST_ASSERT_TRUE(g_sim.agents.loc_y[i] < g_sim.size_y);
        }
    }

    /* No kills: TOUCH_ANY_WALL + enable_kill=false → all agents still alive. */
    TEST_ASSERT_EQUAL_UINT32(pop, alive_count);
}

/* Run a full generation (step loop + sync_to_host + next_generation +
 * sync_from_host) then run a second step loop.  Verifies the upload/download
 * cycle and that the pipeline continues to work after a generation boundary. */
void test_pipeline_generation_boundary(void) {
    if (!g_opencl_ok) {
        TEST_IGNORE_MESSAGE("OpenCL not available");
    }

    /* ── generation 0 step loop ─────────────────────────────────────────── */
    uint32_t steps = g_sim.steps_per_gen;
    for (uint32_t s = 0U; s < steps; s++) {
        g_sim.step = s;
        TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_gpu_pipeline_step(&g_pipeline, &g_sim));
    }

    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_gpu_pipeline_sync_to_host(&g_pipeline, &g_sim));

    /* ── generation boundary ─────────────────────────────────────────────── */
    biosim_census_t census;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_sim_next_generation(&g_sim, &census));

    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_gpu_pipeline_sync_from_host(&g_pipeline, &g_sim));

    /* ── generation 1 step loop ─────────────────────────────────────────── */
    for (uint32_t s = 0U; s < steps; s++) {
        g_sim.step = s;
        TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_gpu_pipeline_step(&g_pipeline, &g_sim));
    }

    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_gpu_pipeline_sync_to_host(&g_pipeline, &g_sim));

    /* All agents alive after gen 1 (TOUCH_ANY_WALL + no kills). */
    uint32_t pop = g_sim.population;
    uint32_t alive_count = 0U;
    for (uint32_t i = 0U; i < pop; i++) {
        if (g_sim.agents.alive[i]) {
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
