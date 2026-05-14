#include "biosim/cfgparse/challenges.h"
#include "biosim/core/params.h"
#include "biosim/core/status.h"
#include "unity.h"

/* clang-format off */
static const biosim_param_entry_t challenge_params[] = {
    {"kind",           "challenge", {.s = "x_band"}, PARAM_STRING, false, true, NULL, NULL},
    {"x-min",          "challenge", {.f = 0.5},      PARAM_FLOAT,  false, true, NULL, NULL},
    {"x-max",          "challenge", {.f = 1.0},      PARAM_FLOAT,  false, true, NULL, NULL},
    {"mirror",         "challenge", {.b = false},    PARAM_BOOL,   false, true, NULL, NULL},
    {"x",             "challenge", {.f = 0.5},      PARAM_FLOAT,  false, true, NULL, NULL},
    {"y",             "challenge", {.f = 0.5},      PARAM_FLOAT,  false, true, NULL, NULL},
    {"radius",         "challenge", {.f = 0.333},    PARAM_FLOAT,  false, true, NULL, NULL},
    {"weighted",       "challenge", {.b = true},     PARAM_BOOL,   false, true, NULL, NULL},
    {"min-n",          "challenge", {.f = 5.0},      PARAM_FLOAT,  false, true, NULL, NULL},
    {"max-n",          "challenge", {.f = 8.0},      PARAM_FLOAT,  false, true, NULL, NULL},
    {"exclude-border", "challenge", {.b = false},    PARAM_BOOL,   false, true, NULL, NULL},
    {"outer-r",        "challenge", {.f = 0.25},     PARAM_FLOAT,  false, true, NULL, NULL},
    {"inner-r",        "challenge", {.f = 0.012},    PARAM_FLOAT,  false, true, NULL, NULL},
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

static void parse_defaults(void) {
    char *argv[] = {"test"};
    int argc = 1;
    TEST_ASSERT_EQUAL(BIOSIM_OK, biosim_params_parse(&p, "test", "0", argc, argv));
}

/* ── x_band defaults ─────────────────────────────────────────────────────── */

void test_xband_defaults(void) {
    parse_defaults();
    biosim_challenge_spec_t spec;
    TEST_ASSERT_EQUAL(BIOSIM_OK, biosim_challenge_spec_from_params(&p, &spec));
    TEST_ASSERT_EQUAL_INT(BIOSIM_CHALLENGE_X_BAND, spec.kind);
    TEST_ASSERT_EQUAL_FLOAT(0.5F, (float)spec.x_band.x_min);
    TEST_ASSERT_EQUAL_FLOAT(1.0F, (float)spec.x_band.x_max);
    TEST_ASSERT_FALSE(spec.x_band.mirror);
}

/* ── disc defaults ───────────────────────────────────────────────────────── */

void test_disc_defaults(void) {
    parse_defaults();
    TEST_ASSERT_EQUAL(BIOSIM_OK, biosim_params_set_string(&p, "kind", "disc"));
    biosim_challenge_spec_t spec;
    TEST_ASSERT_EQUAL(BIOSIM_OK, biosim_challenge_spec_from_params(&p, &spec));
    TEST_ASSERT_EQUAL_INT(BIOSIM_CHALLENGE_DISC, spec.kind);
    TEST_ASSERT_EQUAL_FLOAT(0.5F, (float)spec.disc.x);
    TEST_ASSERT_EQUAL_FLOAT(0.5F, (float)spec.disc.y);
    TEST_ASSERT_FLOAT_WITHIN(0.001F, 0.333F, (float)spec.disc.radius);
    TEST_ASSERT_TRUE(spec.disc.weighted);
}

/* ── corners defaults ────────────────────────────────────────────────────── */

void test_corners_defaults(void) {
    parse_defaults();
    TEST_ASSERT_EQUAL(BIOSIM_OK, biosim_params_set_string(&p, "kind", "corners"));
    biosim_challenge_spec_t spec;
    TEST_ASSERT_EQUAL(BIOSIM_OK, biosim_challenge_spec_from_params(&p, &spec));
    TEST_ASSERT_EQUAL_INT(BIOSIM_CHALLENGE_CORNERS, spec.kind);
    TEST_ASSERT_FLOAT_WITHIN(0.001F, 0.333F, (float)spec.corners.radius);
    TEST_ASSERT_TRUE(spec.corners.weighted);
}

/* ── neighbor_count defaults ─────────────────────────────────────────────── */

void test_neighbor_count_defaults(void) {
    parse_defaults();
    TEST_ASSERT_EQUAL(BIOSIM_OK, biosim_params_set_string(&p, "kind", "neighbor_count"));
    biosim_challenge_spec_t spec;
    TEST_ASSERT_EQUAL(BIOSIM_OK, biosim_challenge_spec_from_params(&p, &spec));
    TEST_ASSERT_EQUAL_INT(BIOSIM_CHALLENGE_NEIGHBOR_COUNT, spec.kind);
    TEST_ASSERT_FLOAT_WITHIN(0.001F, 0.333F, (float)spec.neighbor_count.radius);
    TEST_ASSERT_EQUAL_FLOAT(5.0F, (float)spec.neighbor_count.min_n);
    TEST_ASSERT_EQUAL_FLOAT(8.0F, (float)spec.neighbor_count.max_n);
    TEST_ASSERT_FALSE(spec.neighbor_count.exclude_border);
}

/* ── center_sparse defaults ──────────────────────────────────────────────── */

void test_center_sparse_defaults(void) {
    parse_defaults();
    TEST_ASSERT_EQUAL(BIOSIM_OK, biosim_params_set_string(&p, "kind", "center_sparse"));
    biosim_challenge_spec_t spec;
    TEST_ASSERT_EQUAL(BIOSIM_OK, biosim_challenge_spec_from_params(&p, &spec));
    TEST_ASSERT_EQUAL_INT(BIOSIM_CHALLENGE_CENTER_SPARSE, spec.kind);
    TEST_ASSERT_EQUAL_FLOAT(0.5F, (float)spec.center_sparse.x);
    TEST_ASSERT_EQUAL_FLOAT(0.5F, (float)spec.center_sparse.y);
    TEST_ASSERT_EQUAL_FLOAT(0.25F, (float)spec.center_sparse.outer_r);
    TEST_ASSERT_FLOAT_WITHIN(0.001F, 0.012F, (float)spec.center_sparse.inner_r);
    TEST_ASSERT_EQUAL_FLOAT(5.0F, (float)spec.center_sparse.min_n);
    TEST_ASSERT_EQUAL_FLOAT(8.0F, (float)spec.center_sparse.max_n);
    TEST_ASSERT_TRUE(spec.center_sparse.weighted);
}

/* ── near_barrier defaults ───────────────────────────────────────────────── */

void test_near_barrier_defaults(void) {
    parse_defaults();
    TEST_ASSERT_EQUAL(BIOSIM_OK, biosim_params_set_string(&p, "kind", "near_barrier"));
    biosim_challenge_spec_t spec;
    TEST_ASSERT_EQUAL(BIOSIM_OK, biosim_challenge_spec_from_params(&p, &spec));
    TEST_ASSERT_EQUAL_INT(BIOSIM_CHALLENGE_NEAR_BARRIER, spec.kind);
    TEST_ASSERT_FLOAT_WITHIN(0.001F, 0.333F, (float)spec.near_barrier.radius);
}

/* ── parameter-free kinds parse successfully ─────────────────────────────── */

void test_against_wall_parses(void) {
    parse_defaults();
    TEST_ASSERT_EQUAL(BIOSIM_OK, biosim_params_set_string(&p, "kind", "against_wall"));
    biosim_challenge_spec_t spec;
    TEST_ASSERT_EQUAL(BIOSIM_OK, biosim_challenge_spec_from_params(&p, &spec));
    TEST_ASSERT_EQUAL_INT(BIOSIM_CHALLENGE_AGAINST_WALL, spec.kind);
}

void test_migrate_distance_parses(void) {
    parse_defaults();
    TEST_ASSERT_EQUAL(BIOSIM_OK, biosim_params_set_string(&p, "kind", "migrate_distance"));
    biosim_challenge_spec_t spec;
    TEST_ASSERT_EQUAL(BIOSIM_OK, biosim_challenge_spec_from_params(&p, &spec));
    TEST_ASSERT_EQUAL_INT(BIOSIM_CHALLENGE_MIGRATE_DISTANCE, spec.kind);
}

void test_touch_any_wall_parses(void) {
    parse_defaults();
    TEST_ASSERT_EQUAL(BIOSIM_OK, biosim_params_set_string(&p, "kind", "touch_any_wall"));
    biosim_challenge_spec_t spec;
    TEST_ASSERT_EQUAL(BIOSIM_OK, biosim_challenge_spec_from_params(&p, &spec));
    TEST_ASSERT_EQUAL_INT(BIOSIM_CHALLENGE_TOUCH_ANY_WALL, spec.kind);
}

void test_radioactive_walls_parses(void) {
    parse_defaults();
    TEST_ASSERT_EQUAL(BIOSIM_OK, biosim_params_set_string(&p, "kind", "radioactive_walls"));
    biosim_challenge_spec_t spec;
    TEST_ASSERT_EQUAL(BIOSIM_OK, biosim_challenge_spec_from_params(&p, &spec));
    TEST_ASSERT_EQUAL_INT(BIOSIM_CHALLENGE_RADIOACTIVE_WALLS, spec.kind);
}

void test_pairs_parses(void) {
    parse_defaults();
    TEST_ASSERT_EQUAL(BIOSIM_OK, biosim_params_set_string(&p, "kind", "pairs"));
    biosim_challenge_spec_t spec;
    TEST_ASSERT_EQUAL(BIOSIM_OK, biosim_challenge_spec_from_params(&p, &spec));
    TEST_ASSERT_EQUAL_INT(BIOSIM_CHALLENGE_PAIRS, spec.kind);
}

void test_location_sequence_parses(void) {
    parse_defaults();
    TEST_ASSERT_EQUAL(BIOSIM_OK, biosim_params_set_string(&p, "kind", "location_sequence"));
    biosim_challenge_spec_t spec;
    TEST_ASSERT_EQUAL(BIOSIM_OK, biosim_challenge_spec_from_params(&p, &spec));
    TEST_ASSERT_EQUAL_INT(BIOSIM_CHALLENGE_LOCATION_SEQUENCE, spec.kind);
}

void test_altruism_parses(void) {
    parse_defaults();
    TEST_ASSERT_EQUAL(BIOSIM_OK, biosim_params_set_string(&p, "kind", "altruism"));
    biosim_challenge_spec_t spec;
    TEST_ASSERT_EQUAL(BIOSIM_OK, biosim_challenge_spec_from_params(&p, &spec));
    TEST_ASSERT_EQUAL_INT(BIOSIM_CHALLENGE_ALTRUISM, spec.kind);
}

/* ── unknown kind returns BIOSIM_ERR_INVALID ─────────────────────────────── */

void test_unknown_kind_returns_invalid(void) {
    parse_defaults();
    TEST_ASSERT_EQUAL(BIOSIM_OK, biosim_params_set_string(&p, "kind", "not_a_kind"));
    biosim_challenge_spec_t spec;
    TEST_ASSERT_EQUAL(BIOSIM_ERR_INVALID, biosim_challenge_spec_from_params(&p, &spec));
}

/* ── runner ──────────────────────────────────────────────────────────────── */

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_xband_defaults);
    RUN_TEST(test_disc_defaults);
    RUN_TEST(test_corners_defaults);
    RUN_TEST(test_neighbor_count_defaults);
    RUN_TEST(test_center_sparse_defaults);
    RUN_TEST(test_near_barrier_defaults);
    RUN_TEST(test_against_wall_parses);
    RUN_TEST(test_migrate_distance_parses);
    RUN_TEST(test_touch_any_wall_parses);
    RUN_TEST(test_radioactive_walls_parses);
    RUN_TEST(test_pairs_parses);
    RUN_TEST(test_location_sequence_parses);
    RUN_TEST(test_altruism_parses);
    RUN_TEST(test_unknown_kind_returns_invalid);
    return UNITY_END();
}
