#include "biosim/core/challenges.h"
#include "biosim/core/status.h"
#include "biosim/params/challenges.h"
#include "biosim/params/params.h"
#include "unity.h"

/* clang-format off */
static const biosim_param_entry_t challenge_params[] = {
    {"kind",   "challenge", {.s = "x_band"}, PARAM_STRING, false, true, "challenge-kind",  NULL},
    {"x-min",  "challenge", {.f = 0.5},      PARAM_FLOAT,  false, true, "challenge-x-min", NULL},
    {"x-max",  "challenge", {.f = 1.0},      PARAM_FLOAT,  false, true, "challenge-x-max", NULL},
    {"mirror", "challenge", {.b = false},    PARAM_BOOL,   false, true, "challenge-mirror",NULL},
};
/* clang-format on */
#define CHALLENGE_PARAMS_COUNT (sizeof(challenge_params) / sizeof(challenge_params[0]))

static biosim_params_t p;

void setUp(void) {
    TEST_ASSERT_EQUAL(BIOSIM_OK, biosim_params_init(&p, challenge_params, CHALLENGE_PARAMS_COUNT));
}

void tearDown(void) {
    biosim_params_free(&p);
}

/* ── defaults applied when no config ────────────────────────────────────── */

void test_xband_defaults(void) {
    char *argv[] = {"test"};
    int argc = 1;
    TEST_ASSERT_EQUAL(BIOSIM_OK, biosim_params_parse(&p, "test", "0", argc, argv));

    biosim_challenge_spec_t spec;
    TEST_ASSERT_EQUAL(BIOSIM_OK, biosim_challenge_spec_from_params(&p, &spec));

    TEST_ASSERT_EQUAL_INT(BIOSIM_CHALLENGE_X_BAND, spec.kind);
    TEST_ASSERT_EQUAL_FLOAT(0.5F, (float)spec.x_band.x_min);
    TEST_ASSERT_EQUAL_FLOAT(1.0F, (float)spec.x_band.x_max);
    TEST_ASSERT_FALSE(spec.x_band.mirror);
}

/* ── unknown kind returns BIOSIM_ERR_INVALID ─────────────────────────────── */

void test_unknown_kind_returns_invalid(void) {
    char *argv[] = {"test"};
    int argc = 1;
    TEST_ASSERT_EQUAL(BIOSIM_OK, biosim_params_parse(&p, "test", "0", argc, argv));
    TEST_ASSERT_EQUAL(BIOSIM_OK, biosim_params_set_string(&p, "kind", "not_a_kind"));

    biosim_challenge_spec_t spec;
    TEST_ASSERT_EQUAL(BIOSIM_ERR_INVALID, biosim_challenge_spec_from_params(&p, &spec));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_xband_defaults);
    RUN_TEST(test_unknown_kind_returns_invalid);
    return UNITY_END();
}
