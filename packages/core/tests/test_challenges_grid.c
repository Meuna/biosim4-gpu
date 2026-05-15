#include "biosim/core/challenges.h"
#include "biosim/core/grid.h"
#include "biosim/core/sim.h"
#include "biosim/core/status.h"
#include "biosim/core/test_utils.h"
#include "unity.h"

#include <stdlib.h>

static biosim_sim_t sim;

static biosim_coord_t coord(int32_t x, int32_t y) {
    biosim_coord_t c = {x, y};
    return c;
}

/* Place agent i at (x,y), mark alive, and stamp the grid. */
static void place_agent(uint32_t i, int32_t x, int32_t y) {
    sim.agents.loc_x[i] = x;
    sim.agents.loc_y[i] = y;
    sim.agents.alive[i] = 1;
    biosim_grid_set(&sim.grid, coord(x, y), i + 1U);
}

void setUp(void) {
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, sim_test_create(&sim, &(sim_test_scn_t){
                                                               .population = 10U,
                                                               .size_x = 64,
                                                               .size_y = 64,
                                                               .genome_max_len = 8U,
                                                               .max_neurons = 3U,
                                                               .long_probe_dist = 8U,
                                                           }));
    biosim_grid_zero_fill(&sim.grid);
    for (uint32_t i = 0; i < sim.agents.population; i++) {
        sim.agents.alive[i] = 0;
    }
}

void tearDown(void) {
    biosim_sim_free(&sim);
}

/* ── neighbor_count ──────────────────────────────────────────────────────── */

void test_neighbor_count_in_range_passes(void) {
    /* Agent 0 at (32,32); two neighbours at (33,32) and (34,32).
     * radius = 0.08F → rpx = (int32_t)(0.08 * 64) = 5 pixels.
     * visit_neighborhood counts agent 0 itself plus the two neighbours = 3. */
    place_agent(0, 32, 32);
    place_agent(1, 33, 32);
    place_agent(2, 34, 32);

    biosim_challenge_spec_t s;
    s.kind = BIOSIM_CHALLENGE_NEIGHBOR_COUNT;
    s.neighbor_count.radius = 0.08F;
    s.neighbor_count.min_n = 2;
    s.neighbor_count.max_n = 5;
    s.neighbor_count.exclude_border = false;
    TEST_ASSERT_TRUE(biosim_challenge_eval(&s, 0, &sim).passed);
}

void test_neighbor_count_too_few_fails(void) {
    /* Only agent 0 placed → count = 1 (itself); min_n = 5 → fails */
    place_agent(0, 32, 32);

    biosim_challenge_spec_t s;
    s.kind = BIOSIM_CHALLENGE_NEIGHBOR_COUNT;
    s.neighbor_count.radius = 0.08F;
    s.neighbor_count.min_n = 5;
    s.neighbor_count.max_n = 10;
    s.neighbor_count.exclude_border = false;
    TEST_ASSERT_FALSE(biosim_challenge_eval(&s, 0, &sim).passed);
}

void test_neighbor_count_too_many_fails(void) {
    /* Four agents tightly packed around agent 0 → count = 5; max_n = 3 → fails */
    place_agent(0, 32, 32);
    place_agent(1, 33, 32);
    place_agent(2, 31, 32);
    place_agent(3, 32, 33);
    place_agent(4, 32, 31);

    biosim_challenge_spec_t s;
    s.kind = BIOSIM_CHALLENGE_NEIGHBOR_COUNT;
    s.neighbor_count.radius = 0.08F;
    s.neighbor_count.min_n = 1;
    s.neighbor_count.max_n = 3;
    s.neighbor_count.exclude_border = false;
    TEST_ASSERT_FALSE(biosim_challenge_eval(&s, 0, &sim).passed);
}

void test_neighbor_count_exclude_border_fails(void) {
    place_agent(0, 0, 32);

    biosim_challenge_spec_t s;
    s.kind = BIOSIM_CHALLENGE_NEIGHBOR_COUNT;
    s.neighbor_count.radius = 0.08F;
    s.neighbor_count.min_n = 0;
    s.neighbor_count.max_n = 10;
    s.neighbor_count.exclude_border = true;
    TEST_ASSERT_FALSE(biosim_challenge_eval(&s, 0, &sim).passed);
}

/* ── center_sparse ───────────────────────────────────────────────────────── */

void test_center_sparse_sparse_center_passes(void) {
    /* Agent 0 at center (32,32); no inner neighbours.
     * outer_r = 0.5F → 32 px (covers whole grid half)
     * inner_r = 0.05F → 3 px; only agent 0 itself in inner disc.
     * min_n=0, max_n=3 → count=1 → passes */
    place_agent(0, 32, 32);

    biosim_challenge_spec_t s;
    s.kind = BIOSIM_CHALLENGE_CENTER_SPARSE;
    s.center_sparse.x = 0.5F;
    s.center_sparse.y = 0.5F;
    s.center_sparse.outer_r = 0.5F;
    s.center_sparse.inner_r = 0.05F;
    s.center_sparse.min_n = 0;
    s.center_sparse.max_n = 3;
    s.center_sparse.weighted = false;
    TEST_ASSERT_TRUE(biosim_challenge_eval(&s, 0, &sim).passed);
}

void test_center_sparse_crowded_fails(void) {
    /* Five agents tightly packed → count = 5; max_n = 3 → fails */
    place_agent(0, 32, 32);
    place_agent(1, 33, 32);
    place_agent(2, 31, 32);
    place_agent(3, 32, 33);
    place_agent(4, 32, 31);

    biosim_challenge_spec_t s;
    s.kind = BIOSIM_CHALLENGE_CENTER_SPARSE;
    s.center_sparse.x = 0.5F;
    s.center_sparse.y = 0.5F;
    s.center_sparse.outer_r = 0.5F;
    s.center_sparse.inner_r = 0.05F;
    s.center_sparse.min_n = 0;
    s.center_sparse.max_n = 3;
    s.center_sparse.weighted = false;
    TEST_ASSERT_FALSE(biosim_challenge_eval(&s, 0, &sim).passed);
}

void test_center_sparse_outside_outer_fails(void) {
    /* Agent at (0,0); center at (32,32); outer_r = 0.1F → 6.4 px.
     * Distance ≈ 45 px > 6.4 → fails */
    place_agent(0, 0, 0);

    biosim_challenge_spec_t s;
    s.kind = BIOSIM_CHALLENGE_CENTER_SPARSE;
    s.center_sparse.x = 0.5F;
    s.center_sparse.y = 0.5F;
    s.center_sparse.outer_r = 0.1F;
    s.center_sparse.inner_r = 0.05F;
    s.center_sparse.min_n = 0;
    s.center_sparse.max_n = 10;
    s.center_sparse.weighted = false;
    TEST_ASSERT_FALSE(biosim_challenge_eval(&s, 0, &sim).passed);
}

/* ── pairs ───────────────────────────────────────────────────────────────── */

void test_pairs_valid_pair_passes(void) {
    /* Agent 0 at (32,32); only neighbour = agent 1 at (33,32).
     * Agent 1 has no other neighbours → passes */
    place_agent(0, 32, 32);
    place_agent(1, 33, 32);

    biosim_challenge_spec_t s;
    s.kind = BIOSIM_CHALLENGE_PAIRS;
    TEST_ASSERT_TRUE(biosim_challenge_eval(&s, 0, &sim).passed);
}

void test_pairs_no_neighbour_fails(void) {
    place_agent(0, 32, 32);

    biosim_challenge_spec_t s;
    s.kind = BIOSIM_CHALLENGE_PAIRS;
    TEST_ASSERT_FALSE(biosim_challenge_eval(&s, 0, &sim).passed);
}

void test_pairs_two_neighbours_fails(void) {
    place_agent(0, 32, 32);
    place_agent(1, 33, 32);
    place_agent(2, 32, 33);

    biosim_challenge_spec_t s;
    s.kind = BIOSIM_CHALLENGE_PAIRS;
    TEST_ASSERT_FALSE(biosim_challenge_eval(&s, 0, &sim).passed);
}

void test_pairs_neighbour_has_extra_neighbour_fails(void) {
    /* Agent 1 (neighbour of 0) also has agent 2 as neighbour → 0 fails */
    place_agent(0, 32, 32);
    place_agent(1, 33, 32);
    place_agent(2, 34, 32);

    biosim_challenge_spec_t s;
    s.kind = BIOSIM_CHALLENGE_PAIRS;
    TEST_ASSERT_FALSE(biosim_challenge_eval(&s, 0, &sim).passed);
}

void test_pairs_on_border_fails(void) {
    place_agent(0, 0, 32);
    place_agent(1, 1, 32);

    biosim_challenge_spec_t s;
    s.kind = BIOSIM_CHALLENGE_PAIRS;
    TEST_ASSERT_FALSE(biosim_challenge_eval(&s, 0, &sim).passed);
}

/* ── near_barrier ────────────────────────────────────────────────────────── */

void test_near_barrier_within_radius_passes(void) {
    biosim_coord_t *bctrs = (biosim_coord_t *)malloc(sizeof(biosim_coord_t));
    TEST_ASSERT_NOT_NULL(bctrs);
    bctrs[0].x = 20;
    bctrs[0].y = 20;
    sim.barrier_ctrs = bctrs; /* context_free releases this */
    sim.n_barrier_ctrs = 1;

    sim.agents.loc_x[0] = 20;
    sim.agents.loc_y[0] = 20;
    sim.agents.alive[0] = 1;

    biosim_challenge_spec_t s;
    s.kind = BIOSIM_CHALLENGE_NEAR_BARRIER;
    s.near_barrier.radius = 0.1F; /* 6.4 px */
    TEST_ASSERT_TRUE(biosim_challenge_eval(&s, 0, &sim).passed);
}

void test_near_barrier_outside_radius_fails(void) {
    biosim_coord_t *bctrs = (biosim_coord_t *)malloc(sizeof(biosim_coord_t));
    TEST_ASSERT_NOT_NULL(bctrs);
    bctrs[0].x = 20;
    bctrs[0].y = 20;
    sim.barrier_ctrs = bctrs;
    sim.n_barrier_ctrs = 1;

    sim.agents.loc_x[0] = 0;
    sim.agents.loc_y[0] = 0; /* distance ≈ 28 px; radius = 6.4 px */
    sim.agents.alive[0] = 1;

    biosim_challenge_spec_t s;
    s.kind = BIOSIM_CHALLENGE_NEAR_BARRIER;
    s.near_barrier.radius = 0.1F;
    TEST_ASSERT_FALSE(biosim_challenge_eval(&s, 0, &sim).passed);
}

void test_near_barrier_no_barriers_fails(void) {
    sim.agents.loc_x[0] = 32;
    sim.agents.loc_y[0] = 32;
    sim.agents.alive[0] = 1;

    biosim_challenge_spec_t s;
    s.kind = BIOSIM_CHALLENGE_NEAR_BARRIER;
    s.near_barrier.radius = 0.5F;
    TEST_ASSERT_FALSE(biosim_challenge_eval(&s, 0, &sim).passed);
}

/* ── runner ──────────────────────────────────────────────────────────────── */

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_neighbor_count_in_range_passes);
    RUN_TEST(test_neighbor_count_too_few_fails);
    RUN_TEST(test_neighbor_count_too_many_fails);
    RUN_TEST(test_neighbor_count_exclude_border_fails);
    RUN_TEST(test_center_sparse_sparse_center_passes);
    RUN_TEST(test_center_sparse_crowded_fails);
    RUN_TEST(test_center_sparse_outside_outer_fails);
    RUN_TEST(test_pairs_valid_pair_passes);
    RUN_TEST(test_pairs_no_neighbour_fails);
    RUN_TEST(test_pairs_two_neighbours_fails);
    RUN_TEST(test_pairs_neighbour_has_extra_neighbour_fails);
    RUN_TEST(test_pairs_on_border_fails);
    RUN_TEST(test_near_barrier_within_radius_passes);
    RUN_TEST(test_near_barrier_outside_radius_fails);
    RUN_TEST(test_near_barrier_no_barriers_fails);
    return UNITY_END();
}
