#include "biosim/core/challenges.h"
#include "unity.h"

void setUp(void) {
}
void tearDown(void) {
}

/* ── helpers ────────────────────────────────────────────────────────────── */

static biosim_challenge_spec_t make_xband(float x_min, float x_max, bool mirror) {
    biosim_challenge_spec_t s;
    s.kind = BIOSIM_CHALLENGE_X_BAND;
    s.x_band.x_min = x_min;
    s.x_band.x_max = x_max;
    s.x_band.mirror = mirror;
    return s;
}

/* ── x_band: right half (x_min=0.5, x_max=1.0, mirror=false) ───────────── */

void test_xband_right_half_pass(void) {
    biosim_challenge_spec_t s = make_xband(0.5F, 1.0F, false);
    biosim_challenge_result_t r = biosim_challenge_eval(&s, 64, 0, 128, 128, 0);
    TEST_ASSERT_TRUE(r.passed);
    TEST_ASSERT_EQUAL_FLOAT(1.0F, r.score);
    r = biosim_challenge_eval(&s, 127, 0, 128, 128, 0);
    TEST_ASSERT_TRUE(r.passed);
    TEST_ASSERT_EQUAL_FLOAT(1.0F, r.score);
}

void test_xband_right_half_fail(void) {
    biosim_challenge_spec_t s = make_xband(0.5F, 1.0F, false);
    biosim_challenge_result_t r = biosim_challenge_eval(&s, 63, 0, 128, 128, 0);
    TEST_ASSERT_FALSE(r.passed);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, r.score);
    r = biosim_challenge_eval(&s, 0, 0, 128, 128, 0);
    TEST_ASSERT_FALSE(r.passed);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, r.score);
}

/* ── x_band: left and right eighth (x_min=0.0, x_max=0.125, mirror=true) ───── */

/* mirror: [0, 0.125) (0,875, 1] → x in  [0, 15) (112, 127] */

void test_xband_left_right_left_pass(void) {
    biosim_challenge_spec_t s = make_xband(0.0F, 0.125F, true);
    biosim_challenge_result_t r = biosim_challenge_eval(&s, 0, 0, 128, 128, 0);
    TEST_ASSERT_TRUE(r.passed);
    r = biosim_challenge_eval(&s, 15, 0, 128, 128, 0);
    TEST_ASSERT_TRUE(r.passed);
}

void test_xband_left_right_right_pass(void) {
    biosim_challenge_spec_t s = make_xband(0.0F, 0.125F, true);
    biosim_challenge_result_t r = biosim_challenge_eval(&s, 112, 0, 128, 128, 0);
    TEST_ASSERT_TRUE(r.passed);
    r = biosim_challenge_eval(&s, 127, 0, 128, 128, 0);
    TEST_ASSERT_TRUE(r.passed);
}

void test_xband_left_right_center_fail(void) {
    biosim_challenge_spec_t s = make_xband(0.0F, 0.125F, true);
    biosim_challenge_result_t r = biosim_challenge_eval(&s, 17, 0, 128, 128, 0);
    TEST_ASSERT_FALSE(r.passed);
    r = biosim_challenge_eval(&s, 111, 0, 128, 128, 0);
    TEST_ASSERT_FALSE(r.passed);
}

/* ── unimplemented kinds return stub ─────────────────────────────────────── */

void test_unimplemented_kind_returns_fail(void) {
    biosim_challenge_spec_t s;
    s.kind = BIOSIM_CHALLENGE_DISC;
    s.disc.cx = 0.5F;
    s.disc.cy = 0.5F;
    s.disc.radius = 0.333F;
    s.disc.weighted = true;
    biosim_challenge_result_t r = biosim_challenge_eval(&s, 64, 64, 128, 128, 0);
    TEST_ASSERT_FALSE(r.passed);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, r.score);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_xband_right_half_pass);
    RUN_TEST(test_xband_right_half_fail);
    RUN_TEST(test_xband_left_right_left_pass);
    RUN_TEST(test_xband_left_right_right_pass);
    RUN_TEST(test_xband_left_right_center_fail);
    RUN_TEST(test_unimplemented_kind_returns_fail);
    return UNITY_END();
}
