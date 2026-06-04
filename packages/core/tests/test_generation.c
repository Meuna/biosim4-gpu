#include "biosim/core/census.h"
#include "biosim/core/generation.h"
#include "biosim/core/sim.h"
#include "biosim/core/survivor_snap.h"
#include "biosim/core/test_utils.h"
#include "unity.h"

void setUp(void) {
}
void tearDown(void) {
}

/*
 * 8x8 sim uses X_BAND challenge [0, 1] covering the full grid, so all 4
 * alive agents pass.
 */
void test_collect_survivors_all_pass(void) {
    biosim_sim_t sim;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, sim_test_make_8x8(&sim));

    biosim_survivor_snap_t snap = {0};
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_generation_collect_survivors(&sim, &snap));
    TEST_ASSERT_EQUAL_UINT32(4U, snap.count);
    TEST_ASSERT_NOT_NULL(snap.conn);
    TEST_ASSERT_NOT_NULL(snap.len);
    TEST_ASSERT_NOT_NULL(snap.wgt);
    TEST_ASSERT_NOT_NULL(snap.scores);

    biosim_survivor_snap_free(&snap);
    biosim_sim_free(&sim);
}

void test_collect_survivors_sets_genome_data(void) {
    biosim_sim_t sim;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, sim_test_make_8x8(&sim));

    biosim_survivor_snap_t snap = {0};
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_generation_collect_survivors(&sim, &snap));

    for (uint32_t s = 0U; s < snap.count; s++) {
        TEST_ASSERT_TRUE(snap.len[s] > 0U);
        TEST_ASSERT_TRUE(snap.len[s] <= sim.genome.max_len);
        TEST_ASSERT_TRUE(snap.scores[s] >= 0.0F);
        TEST_ASSERT_TRUE(snap.scores[s] <= 1.0F);
    }

    biosim_survivor_snap_free(&snap);
    biosim_sim_free(&sim);
}

void test_breed_produces_full_population(void) {
    biosim_sim_t sim;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, sim_test_make_8x8(&sim));

    biosim_survivor_snap_t snap = {0};
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_generation_collect_survivors(&sim, &snap));
    TEST_ASSERT_TRUE(snap.count > 0U);
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_generation_breed(&sim, &snap));

    uint32_t alive_count = 0U;
    for (uint32_t i = 0U; i < sim.agents.population; i++) {
        if (sim.agents.alive[i]) {
            alive_count++;
        }
    }
    TEST_ASSERT_EQUAL_UINT32(4U, alive_count);

    biosim_survivor_snap_free(&snap);
    biosim_sim_free(&sim);
}

void test_spawn_with_survivors_breeds(void) {
    biosim_sim_t sim;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, sim_test_make_8x8(&sim));

    biosim_survivor_snap_t snap = {0};
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_generation_collect_survivors(&sim, &snap));
    TEST_ASSERT_TRUE(snap.count > 0U);
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_generation_spawn(&sim, &snap));

    uint32_t alive_count = 0U;
    for (uint32_t i = 0U; i < sim.agents.population; i++) {
        if (sim.agents.alive[i]) {
            alive_count++;
        }
    }
    TEST_ASSERT_EQUAL_UINT32(4U, alive_count);

    biosim_survivor_snap_free(&snap);
    biosim_sim_free(&sim);
}

void test_spawn_with_zero_survivors_init_random(void) {
    biosim_sim_t sim;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, sim_test_make_8x8(&sim));

    biosim_survivor_snap_t snap = {0}; /* count == 0 */
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_generation_spawn(&sim, &snap));

    uint32_t alive_count = 0U;
    for (uint32_t i = 0U; i < sim.agents.population; i++) {
        if (sim.agents.alive[i]) {
            alive_count++;
        }
    }
    TEST_ASSERT_EQUAL_UINT32(4U, alive_count);

    biosim_survivor_snap_free(&snap);
    biosim_sim_free(&sim);
}

/*
 * Run three full generation cycles through the new retire/spawn API and
 * confirm the sim remains consistent throughout.
 */
void test_full_generation_cycle(void) {
    biosim_sim_t sim;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, sim_test_make_32x32(&sim));

    biosim_survivor_snap_t snap = {0};
    biosim_census_t census;

    for (uint32_t g = 0U; g < 3U; g++) {
        sim_test_run_one_gen(&sim);
        TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_sim_retire_generation(&sim, &snap, &census));
        TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_generation_spawn(&sim, &snap));
    }

    TEST_ASSERT_EQUAL_UINT32(3U, sim.gen);

    biosim_survivor_snap_free(&snap);
    biosim_sim_free(&sim);
}

/*
 * Exercise sort_snap_order_by_score: collect + breed with fitness-biased
 * parent selection.  All 4 agents must be alive after breed.
 */
void test_breed_fitness_biased(void) {
    biosim_sim_t sim;
    TEST_ASSERT_EQUAL_INT(
        BIOSIM_OK,
        sim_test_create(
            &sim,
            &(sim_test_scn_t){
                .population = 4U,
                .size_x = 4,
                .size_y = 4,
                .genome_max_len = 4U,
                .max_neurons = 2U,
                .los_range = 4U,
                .steps_per_gen = 1U,
                .sensor_radius = 1,
                .choose_parents_by_fitness = true,
            }
        )
    );

    biosim_survivor_snap_t snap = {0};
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_generation_collect_survivors(&sim, &snap));
    TEST_ASSERT_TRUE(snap.count > 1U); /* need >1 for fitness path to branch */
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_generation_breed(&sim, &snap));

    uint32_t alive_count = 0U;
    for (uint32_t i = 0U; i < sim.agents.population; i++) {
        if (sim.agents.alive[i]) {
            alive_count++;
        }
    }
    TEST_ASSERT_EQUAL_UINT32(4U, alive_count);

    biosim_survivor_snap_free(&snap);
    biosim_sim_free(&sim);
}

/*
 * Exercise crossover_from_snapshot: collect + breed with sexual reproduction.
 * All 4 agents must be alive after breed.
 */
void test_breed_sexual_reproduction(void) {
    biosim_sim_t sim;
    TEST_ASSERT_EQUAL_INT(
        BIOSIM_OK,
        sim_test_create(
            &sim,
            &(sim_test_scn_t){
                .population = 4U,
                .size_x = 4,
                .size_y = 4,
                .genome_max_len = 4U,
                .max_neurons = 2U,
                .los_range = 4U,
                .steps_per_gen = 1U,
                .sensor_radius = 1,
                .sexual_reproduction = true,
            }
        )
    );

    biosim_survivor_snap_t snap = {0};
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_generation_collect_survivors(&sim, &snap));
    TEST_ASSERT_TRUE(snap.count > 0U);
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_generation_breed(&sim, &snap));

    uint32_t alive_count = 0U;
    for (uint32_t i = 0U; i < sim.agents.population; i++) {
        if (sim.agents.alive[i]) {
            alive_count++;
        }
    }
    TEST_ASSERT_EQUAL_UINT32(4U, alive_count);

    biosim_survivor_snap_free(&snap);
    biosim_sim_free(&sim);
}

/* ── runner ─────────────────────────────────────────────────────────────── */

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_collect_survivors_all_pass);
    RUN_TEST(test_collect_survivors_sets_genome_data);
    RUN_TEST(test_breed_produces_full_population);
    RUN_TEST(test_breed_fitness_biased);
    RUN_TEST(test_breed_sexual_reproduction);
    RUN_TEST(test_spawn_with_survivors_breeds);
    RUN_TEST(test_spawn_with_zero_survivors_init_random);
    RUN_TEST(test_full_generation_cycle);
    return UNITY_END();
}
