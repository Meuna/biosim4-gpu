#include "biosim/core/barriers.h"
#include "biosim/core/status.h"
#include "biosim/params/barriers.h"
#include "unity.h"

#include <stdlib.h>

void setUp(void) {
}
void tearDown(void) {
}

/* ── full config ────────────────────────────────────────────────────────── */

void test_full_config_loads_four_specs(void) {
    biosim_barrier_spec_t *specs = NULL;
    int n = 0;
    biosim_status_t st =
        biosim_barrier_params_load(TEST_FIXTURES_DIR "/barriers_full.toml", &specs, &n);
    TEST_ASSERT_EQUAL(BIOSIM_OK, st);
    TEST_ASSERT_EQUAL_INT(4, n);
    TEST_ASSERT_NOT_NULL(specs);

    TEST_ASSERT_EQUAL_INT(BIOSIM_BARRIER_HBAR, specs[0].kind);
    TEST_ASSERT_EQUAL_INT(64, specs[0].x);
    TEST_ASSERT_EQUAL_INT(32, specs[0].y);
    TEST_ASSERT_EQUAL_FLOAT(40.0F, specs[0].length);
    TEST_ASSERT_EQUAL_FLOAT(2.0F, specs[0].width);

    TEST_ASSERT_EQUAL_INT(BIOSIM_BARRIER_VBAR, specs[1].kind);
    TEST_ASSERT_EQUAL_INT(96, specs[1].x);
    TEST_ASSERT_EQUAL_INT(64, specs[1].y);
    TEST_ASSERT_EQUAL_FLOAT(30.0F, specs[1].length);
    TEST_ASSERT_EQUAL_FLOAT(3.0F, specs[1].width);

    TEST_ASSERT_EQUAL_INT(BIOSIM_BARRIER_SQUARE, specs[2].kind);
    TEST_ASSERT_EQUAL_INT(20, specs[2].x);
    TEST_ASSERT_EQUAL_INT(20, specs[2].y);
    TEST_ASSERT_EQUAL_FLOAT(12.0F, specs[2].length);

    TEST_ASSERT_EQUAL_INT(BIOSIM_BARRIER_CIRCLE, specs[3].kind);
    TEST_ASSERT_EQUAL_INT(100, specs[3].x);
    TEST_ASSERT_EQUAL_INT(80, specs[3].y);
    TEST_ASSERT_EQUAL_FLOAT(7.5F, specs[3].length);

    free(specs);
}

/* ── partial config (omitted optional fields become sentinels) ───────────── */

void test_partial_config_sentinels(void) {
    biosim_barrier_spec_t *specs = NULL;
    int n = 0;
    biosim_status_t st =
        biosim_barrier_params_load(TEST_FIXTURES_DIR "/barriers_partial.toml", &specs, &n);
    TEST_ASSERT_EQUAL(BIOSIM_OK, st);
    TEST_ASSERT_EQUAL_INT(2, n);
    TEST_ASSERT_NOT_NULL(specs);

    TEST_ASSERT_EQUAL_INT(BIOSIM_BARRIER_HBAR, specs[0].kind);
    TEST_ASSERT_EQUAL_INT(64, specs[0].x);
    TEST_ASSERT_EQUAL_INT(BIOSIM_BARRIER_POS_UNSET, specs[0].y);
    TEST_ASSERT_EQUAL_FLOAT(BIOSIM_BARRIER_DIM_UNSET, specs[0].length);
    TEST_ASSERT_EQUAL_FLOAT(BIOSIM_BARRIER_DIM_UNSET, specs[0].width);

    TEST_ASSERT_EQUAL_INT(BIOSIM_BARRIER_CIRCLE, specs[1].kind);
    TEST_ASSERT_EQUAL_INT(BIOSIM_BARRIER_POS_UNSET, specs[1].x);
    TEST_ASSERT_EQUAL_INT(BIOSIM_BARRIER_POS_UNSET, specs[1].y);
    TEST_ASSERT_EQUAL_FLOAT(BIOSIM_BARRIER_DIM_UNSET, specs[1].length);

    free(specs);
}

/* ── absence / zero ─────────────────────────────────────────────────────── */

void test_null_path_returns_zero(void) {
    biosim_barrier_spec_t *specs = NULL;
    int n = -1;
    biosim_status_t st = biosim_barrier_params_load(NULL, &specs, &n);
    TEST_ASSERT_EQUAL(BIOSIM_OK, st);
    TEST_ASSERT_EQUAL_INT(0, n);
    TEST_ASSERT_NULL(specs);
}

void test_no_barriers_section_returns_zero(void) {
    biosim_barrier_spec_t *specs = NULL;
    int n = -1;
    /* basic.toml has no [barriers] section */
    biosim_status_t st = biosim_barrier_params_load(TEST_FIXTURES_DIR "/basic.toml", &specs, &n);
    TEST_ASSERT_EQUAL(BIOSIM_OK, st);
    TEST_ASSERT_EQUAL_INT(0, n);
    TEST_ASSERT_NULL(specs);
}

/* runner ─────────────────────────────────────────────────────────────── */

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_full_config_loads_four_specs);
    RUN_TEST(test_partial_config_sentinels);
    RUN_TEST(test_null_path_returns_zero);
    RUN_TEST(test_no_barriers_section_returns_zero);
    return UNITY_END();
}
