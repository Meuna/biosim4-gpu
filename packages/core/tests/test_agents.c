#include "biosim/core/agents.h"
#include "biosim/core/rng.h"
#include "biosim/core/status.h"
#include "unity.h"

static biosim_agents_t agents;

void setUp(void) {
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_agents_create(8, &agents));
}

void tearDown(void) {
    biosim_agents_free(&agents);
}

static biosim_coord_t coord(int32_t x, int32_t y) {
    biosim_coord_t c = {x, y};
    return c;
}

/* ── lifecycle ──────────────────────────────────────────────────────────── */

void test_create_returns_ok(void) {
    biosim_agents_t a;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_agents_create(4, &a));
    biosim_agents_free(&a);
}

void test_create_position_buffers_non_null(void) {
    TEST_ASSERT_NOT_NULL(agents.loc_x);
    TEST_ASSERT_NOT_NULL(agents.loc_y);
    TEST_ASSERT_NOT_NULL(agents.birth_x);
    TEST_ASSERT_NOT_NULL(agents.birth_y);
    TEST_ASSERT_NOT_NULL(agents.desired_x);
    TEST_ASSERT_NOT_NULL(agents.desired_y);
}

void test_create_state_buffers_non_null(void) {
    TEST_ASSERT_NOT_NULL(agents.alive);
    TEST_ASSERT_NOT_NULL(agents.osc_period);
    TEST_ASSERT_NOT_NULL(agents.responsiveness);
    TEST_ASSERT_NOT_NULL(agents.long_probe_dist);
}

void test_create_misc_buffers_non_null(void) {
    TEST_ASSERT_NOT_NULL(agents.last_move_dir);
    TEST_ASSERT_NOT_NULL(agents.challenge_bits);
    TEST_ASSERT_NOT_NULL(agents.rng_state);
    TEST_ASSERT_NOT_NULL(agents.genome_fingerprint);
}

void test_create_capacity_stored(void) {
    TEST_ASSERT_EQUAL_UINT32(8, agents.population);
}

void test_create_all_slots_dead(void) {
    for (uint32_t i = 0; i < agents.population; i++) {
        TEST_ASSERT_EQUAL_UINT8(0, agents.alive[i]);
    }
}

void test_free_clears_struct(void) {
    biosim_agents_t a;
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_agents_create(4, &a));
    biosim_agents_free(&a);
    TEST_ASSERT_NULL(a.loc_x);
    TEST_ASSERT_EQUAL_UINT32(0, a.population);
}

/* ── init_slot ──────────────────────────────────────────────────────────── */

void test_init_slot_marks_alive(void) {
    biosim_agents_init_slot(&agents, 3, coord(5, 7), 16, 42);
    TEST_ASSERT_EQUAL_UINT8(1, agents.alive[3]);
}

void test_init_slot_sets_position(void) {
    biosim_agents_init_slot(&agents, 2, coord(10, 20), 16, 1);
    TEST_ASSERT_EQUAL_INT16(10, agents.loc_x[2]);
    TEST_ASSERT_EQUAL_INT16(20, agents.loc_y[2]);
    TEST_ASSERT_EQUAL_INT16(10, agents.birth_x[2]);
    TEST_ASSERT_EQUAL_INT16(20, agents.birth_y[2]);
}

void test_init_slot_biological_defaults(void) {
    biosim_agents_init_slot(&agents, 0, coord(0, 0), 16, 1);
    TEST_ASSERT_EQUAL_UINT16(34, agents.osc_period[0]);
    TEST_ASSERT_FLOAT_WITHIN(1e-6F, 0.5F, agents.responsiveness[0]);
    TEST_ASSERT_EQUAL_UINT8(16, agents.long_probe_dist[0]);
    TEST_ASSERT_EQUAL_UINT32(0, agents.challenge_bits[0]);
}

void test_init_slot_rng_state_non_zero(void) {
    biosim_agents_init_slot(&agents, 1, coord(1, 1), 8, 0);
    TEST_ASSERT_NOT_EQUAL_UINT64(0, agents.rng_state[1]);
}

void test_init_slot_adjacent_rng_states_differ(void) {
    biosim_agents_init_slot(&agents, 0, coord(0, 0), 8, 99);
    biosim_agents_init_slot(&agents, 1, coord(1, 0), 8, 99);
    TEST_ASSERT_NOT_EQUAL_UINT64(agents.rng_state[0], agents.rng_state[1]);
}

void test_uninitialised_slots_unaffected(void) {
    biosim_agents_init_slot(&agents, 4, coord(3, 3), 16, 7);
    TEST_ASSERT_EQUAL_UINT8(0, agents.alive[5]);
    TEST_ASSERT_EQUAL_UINT8(0, agents.alive[3]);
}

/* ── runner ─────────────────────────────────────────────────────────────── */

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_create_returns_ok);
    RUN_TEST(test_create_position_buffers_non_null);
    RUN_TEST(test_create_state_buffers_non_null);
    RUN_TEST(test_create_misc_buffers_non_null);
    RUN_TEST(test_create_capacity_stored);
    RUN_TEST(test_create_all_slots_dead);
    RUN_TEST(test_free_clears_struct);
    RUN_TEST(test_init_slot_marks_alive);
    RUN_TEST(test_init_slot_sets_position);
    RUN_TEST(test_init_slot_biological_defaults);
    RUN_TEST(test_init_slot_rng_state_non_zero);
    RUN_TEST(test_init_slot_adjacent_rng_states_differ);
    RUN_TEST(test_uninitialised_slots_unaffected);
    return UNITY_END();
}
