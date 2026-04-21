#include "biosim/core/rng.h"
#include "unity.h"

void setUp(void) {
}
void tearDown(void) {
}

/* ── Tests ──────────────────────────────────────────────────────────────── */

void test_next_advances_state(void) {
    uint64_t state = 0xdeadbeefcafeULL;
    for (int i = 0; i < 1000; i++) {
        uint64_t prev = state;
        uint64_t val = biosim_rng_next(&state);
        TEST_ASSERT_NOT_EQUAL_UINT64(prev, state);
        TEST_ASSERT_EQUAL_UINT64(state, val);
    }
}

void test_next_is_deterministic(void) {
    uint64_t s1 = 42;
    uint64_t s2 = 42;
    TEST_ASSERT_EQUAL_UINT64(biosim_rng_next(&s1), biosim_rng_next(&s2));
    TEST_ASSERT_EQUAL_UINT64(biosim_rng_next(&s1), biosim_rng_next(&s2));
}

void test_seed_never_zero(void) {
    /* (0, 0) provably triggers the zero-guard */
    TEST_ASSERT_NOT_EQUAL_UINT64(0, biosim_rng_seed(0, 0));
}

void test_seed_different_indices_differ(void) {
    uint64_t s0 = biosim_rng_seed(0, 1);
    uint64_t s1 = biosim_rng_seed(1, 1);
    uint64_t s2 = biosim_rng_seed(2, 1);
    TEST_ASSERT_NOT_EQUAL_UINT64(s0, s1);
    TEST_ASSERT_NOT_EQUAL_UINT64(s1, s2);
    TEST_ASSERT_NOT_EQUAL_UINT64(s0, s2);
}

/* ── Runner ─────────────────────────────────────────────────────────────── */

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_next_advances_state);
    RUN_TEST(test_next_is_deterministic);
    RUN_TEST(test_seed_never_zero);
    RUN_TEST(test_seed_different_indices_differ);
    return UNITY_END();
}
