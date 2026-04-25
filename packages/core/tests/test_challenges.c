#include "biosim/core/challenges.h"
#include "biosim/core/context.h"
#include "biosim/core/status.h"
#include "unity.h"

#include <stdlib.h>

static biosim_context_t ctx;

void setUp(void) {
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_context_create(1, 128, 128, 8, 3, 8, NULL, 0, &ctx));
    ctx.agents.loc_x[0] = 64;
    ctx.agents.loc_y[0] = 64;
    ctx.agents.birth_x[0] = 64;
    ctx.agents.birth_y[0] = 64;
    ctx.agents.challenge_bits[0] = 0;
    ctx.agents.alive[0] = 1;
}

void tearDown(void) {
    biosim_context_free(&ctx);
}

/* ── x_band ──────────────────────────────────────────────────────────────── */

void test_xband_right_half_pass(void) {
    biosim_challenge_spec_t s;
    s.kind = BIOSIM_CHALLENGE_X_BAND;
    s.x_band.x_min = 0.5F;
    s.x_band.x_max = 1.0F;
    s.x_band.mirror = false;
    ctx.agents.loc_x[0] = 64;
    TEST_ASSERT_TRUE(biosim_challenge_eval(&s, 0, &ctx).passed);
    ctx.agents.loc_x[0] = 127;
    TEST_ASSERT_TRUE(biosim_challenge_eval(&s, 0, &ctx).passed);
}

void test_xband_right_half_fail(void) {
    biosim_challenge_spec_t s;
    s.kind = BIOSIM_CHALLENGE_X_BAND;
    s.x_band.x_min = 0.5F;
    s.x_band.x_max = 1.0F;
    s.x_band.mirror = false;
    ctx.agents.loc_x[0] = 63;
    TEST_ASSERT_FALSE(biosim_challenge_eval(&s, 0, &ctx).passed);
    ctx.agents.loc_x[0] = 0;
    TEST_ASSERT_FALSE(biosim_challenge_eval(&s, 0, &ctx).passed);
}

void test_xband_mirror_left_pass(void) {
    biosim_challenge_spec_t s;
    s.kind = BIOSIM_CHALLENGE_X_BAND;
    s.x_band.x_min = 0.0F;
    s.x_band.x_max = 0.125F;
    s.x_band.mirror = true;
    ctx.agents.loc_x[0] = 0;
    TEST_ASSERT_TRUE(biosim_challenge_eval(&s, 0, &ctx).passed);
    ctx.agents.loc_x[0] = 15;
    TEST_ASSERT_TRUE(biosim_challenge_eval(&s, 0, &ctx).passed);
}

void test_xband_mirror_right_pass(void) {
    biosim_challenge_spec_t s;
    s.kind = BIOSIM_CHALLENGE_X_BAND;
    s.x_band.x_min = 0.0F;
    s.x_band.x_max = 0.125F;
    s.x_band.mirror = true;
    ctx.agents.loc_x[0] = 112;
    TEST_ASSERT_TRUE(biosim_challenge_eval(&s, 0, &ctx).passed);
    ctx.agents.loc_x[0] = 127;
    TEST_ASSERT_TRUE(biosim_challenge_eval(&s, 0, &ctx).passed);
}

void test_xband_mirror_center_fail(void) {
    biosim_challenge_spec_t s;
    s.kind = BIOSIM_CHALLENGE_X_BAND;
    s.x_band.x_min = 0.0F;
    s.x_band.x_max = 0.125F;
    s.x_band.mirror = true;
    ctx.agents.loc_x[0] = 17;
    TEST_ASSERT_FALSE(biosim_challenge_eval(&s, 0, &ctx).passed);
    ctx.agents.loc_x[0] = 111;
    TEST_ASSERT_FALSE(biosim_challenge_eval(&s, 0, &ctx).passed);
}

/* ── disc ────────────────────────────────────────────────────────────────── */

void test_disc_center_passes(void) {
    biosim_challenge_spec_t s;
    s.kind = BIOSIM_CHALLENGE_DISC;
    s.disc.x = 0.5F;
    s.disc.y = 0.5F;
    s.disc.radius = 0.25F;
    s.disc.weighted = false;
    ctx.agents.loc_x[0] = 64;
    ctx.agents.loc_y[0] = 64;
    biosim_challenge_result_t r = biosim_challenge_eval(&s, 0, &ctx);
    TEST_ASSERT_TRUE(r.passed);
    TEST_ASSERT_EQUAL_FLOAT(1.0F, r.score);
}

void test_disc_outside_fails(void) {
    biosim_challenge_spec_t s;
    s.kind = BIOSIM_CHALLENGE_DISC;
    s.disc.x = 0.5F;
    s.disc.y = 0.5F;
    s.disc.radius = 0.1F;
    s.disc.weighted = false;
    ctx.agents.loc_x[0] = 0;
    ctx.agents.loc_y[0] = 0;
    TEST_ASSERT_FALSE(biosim_challenge_eval(&s, 0, &ctx).passed);
}

void test_disc_weighted_center_is_one(void) {
    biosim_challenge_spec_t s;
    s.kind = BIOSIM_CHALLENGE_DISC;
    s.disc.x = 0.5F;
    s.disc.y = 0.5F;
    s.disc.radius = 0.5F;
    s.disc.weighted = true;
    ctx.agents.loc_x[0] = 64;
    ctx.agents.loc_y[0] = 64;
    biosim_challenge_result_t r = biosim_challenge_eval(&s, 0, &ctx);
    TEST_ASSERT_TRUE(r.passed);
    TEST_ASSERT_EQUAL_FLOAT(1.0F, r.score);
}

void test_disc_weighted_corner_is_less_than_one(void) {
    biosim_challenge_spec_t s;
    s.kind = BIOSIM_CHALLENGE_DISC;
    s.disc.x = 0.5F;
    s.disc.y = 0.5F;
    s.disc.radius = 1.0F;
    s.disc.weighted = true;
    ctx.agents.loc_x[0] = 0;
    ctx.agents.loc_y[0] = 0;
    biosim_challenge_result_t r = biosim_challenge_eval(&s, 0, &ctx);
    TEST_ASSERT_TRUE(r.passed);
    TEST_ASSERT_LESS_THAN_FLOAT(1.0F, r.score);
}

/* ── corners ─────────────────────────────────────────────────────────────── */

void test_corners_at_corner_passes(void) {
    biosim_challenge_spec_t s;
    s.kind = BIOSIM_CHALLENGE_CORNERS;
    s.corners.radius = 0.1F;
    s.corners.weighted = false;
    ctx.agents.loc_x[0] = 0;
    ctx.agents.loc_y[0] = 0;
    TEST_ASSERT_TRUE(biosim_challenge_eval(&s, 0, &ctx).passed);
}

void test_corners_center_fails(void) {
    biosim_challenge_spec_t s;
    s.kind = BIOSIM_CHALLENGE_CORNERS;
    s.corners.radius = 0.1F;
    s.corners.weighted = false;
    ctx.agents.loc_x[0] = 64;
    ctx.agents.loc_y[0] = 64;
    TEST_ASSERT_FALSE(biosim_challenge_eval(&s, 0, &ctx).passed);
}

void test_corners_weighted_corner_is_one(void) {
    biosim_challenge_spec_t s;
    s.kind = BIOSIM_CHALLENGE_CORNERS;
    s.corners.radius = 0.1F;
    s.corners.weighted = true;
    ctx.agents.loc_x[0] = 0;
    ctx.agents.loc_y[0] = 0;
    biosim_challenge_result_t r = biosim_challenge_eval(&s, 0, &ctx);
    TEST_ASSERT_TRUE(r.passed);
    TEST_ASSERT_EQUAL_FLOAT(1.0F, r.score);
}

void test_corners_weighted_corner_is_less_than_one(void) {
    biosim_challenge_spec_t s;
    s.kind = BIOSIM_CHALLENGE_CORNERS;
    s.corners.radius = 0.1F;
    s.corners.weighted = true;
    ctx.agents.loc_x[0] = 1;
    ctx.agents.loc_y[0] = 1;
    biosim_challenge_result_t r = biosim_challenge_eval(&s, 0, &ctx);
    TEST_ASSERT_TRUE(r.passed);
    TEST_ASSERT_LESS_THAN_FLOAT(1.0F, r.score);
}

/* ── against_wall ────────────────────────────────────────────────────────── */

void test_against_wall_border_passes(void) {
    biosim_challenge_spec_t s;
    s.kind = BIOSIM_CHALLENGE_AGAINST_WALL;
    ctx.agents.loc_x[0] = 0;
    ctx.agents.loc_y[0] = 32;
    biosim_challenge_result_t r = biosim_challenge_eval(&s, 0, &ctx);
    TEST_ASSERT_TRUE(r.passed);
    TEST_ASSERT_EQUAL_FLOAT(1.0F, r.score);
}

void test_against_wall_interior_fails(void) {
    biosim_challenge_spec_t s;
    s.kind = BIOSIM_CHALLENGE_AGAINST_WALL;
    ctx.agents.loc_x[0] = 64;
    ctx.agents.loc_y[0] = 64;
    TEST_ASSERT_FALSE(biosim_challenge_eval(&s, 0, &ctx).passed);
}

/* ── migrate_distance ────────────────────────────────────────────────────── */

void test_migrate_distance_no_move_zero_score(void) {
    biosim_challenge_spec_t s;
    s.kind = BIOSIM_CHALLENGE_MIGRATE_DISTANCE;
    ctx.agents.loc_x[0] = 64;
    ctx.agents.loc_y[0] = 64;
    ctx.agents.birth_x[0] = 64;
    ctx.agents.birth_y[0] = 64;
    biosim_challenge_result_t r = biosim_challenge_eval(&s, 0, &ctx);
    TEST_ASSERT_TRUE(r.passed);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, r.score);
}

void test_migrate_distance_moved_positive_score(void) {
    biosim_challenge_spec_t s;
    s.kind = BIOSIM_CHALLENGE_MIGRATE_DISTANCE;
    ctx.agents.loc_x[0] = 0;
    ctx.agents.loc_y[0] = 0;
    ctx.agents.birth_x[0] = 64;
    ctx.agents.birth_y[0] = 64;
    biosim_challenge_result_t r = biosim_challenge_eval(&s, 0, &ctx);
    TEST_ASSERT_TRUE(r.passed);
    TEST_ASSERT_GREATER_THAN_FLOAT(0.0F, r.score);
}

/* ── touch_any_wall eval ─────────────────────────────────────────────────── */

void test_touch_any_wall_bits_zero_fails(void) {
    biosim_challenge_spec_t s;
    s.kind = BIOSIM_CHALLENGE_TOUCH_ANY_WALL;
    ctx.agents.challenge_bits[0] = 0;
    TEST_ASSERT_FALSE(biosim_challenge_eval(&s, 0, &ctx).passed);
}

void test_touch_any_wall_bits_set_passes(void) {
    biosim_challenge_spec_t s;
    s.kind = BIOSIM_CHALLENGE_TOUCH_ANY_WALL;
    ctx.agents.challenge_bits[0] = 1;
    biosim_challenge_result_t r = biosim_challenge_eval(&s, 0, &ctx);
    TEST_ASSERT_TRUE(r.passed);
    TEST_ASSERT_EQUAL_FLOAT(1.0F, r.score);
}

/* ── touch_any_wall step hook ────────────────────────────────────────────── */

void test_step_touch_any_wall_on_border_sets_bit(void) {
    biosim_challenge_spec_t s;
    s.kind = BIOSIM_CHALLENGE_TOUCH_ANY_WALL;
    ctx.agents.loc_x[0] = 0;
    ctx.agents.loc_y[0] = 32;
    ctx.agents.challenge_bits[0] = 0;
    biosim_challenge_step(&s, &ctx, 0, 300);
    TEST_ASSERT_NOT_EQUAL(0U, ctx.agents.challenge_bits[0]);
}

void test_step_touch_any_wall_interior_no_bit(void) {
    biosim_challenge_spec_t s;
    s.kind = BIOSIM_CHALLENGE_TOUCH_ANY_WALL;
    ctx.agents.loc_x[0] = 32;
    ctx.agents.loc_y[0] = 32;
    ctx.agents.challenge_bits[0] = 0;
    biosim_challenge_step(&s, &ctx, 0, 300);
    TEST_ASSERT_EQUAL(0U, ctx.agents.challenge_bits[0]);
}

/* ── radioactive_walls eval ──────────────────────────────────────────────── */

void test_radioactive_walls_always_passes_eval(void) {
    biosim_challenge_spec_t s;
    s.kind = BIOSIM_CHALLENGE_RADIOACTIVE_WALLS;
    biosim_challenge_result_t r = biosim_challenge_eval(&s, 0, &ctx);
    TEST_ASSERT_TRUE(r.passed);
    TEST_ASSERT_EQUAL_FLOAT(1.0F, r.score);
}

/* ── radioactive_walls step hook ─────────────────────────────────────────── */

void test_step_radioactive_walls_kills_at_wall(void) {
    biosim_challenge_spec_t s;
    s.kind = BIOSIM_CHALLENGE_RADIOACTIVE_WALLS;
    /* sim_step=0 < steps_per_gen/2 → radioactive_x = 0 */
    ctx.agents.loc_x[0] = 0;
    ctx.agents.alive[0] = 1;
    biosim_challenge_step(&s, &ctx, 0, 300);
    TEST_ASSERT_EQUAL(0, ctx.agents.alive[0]);
}

/* ── location_sequence eval ──────────────────────────────────────────────── */

void test_location_sequence_no_bits_fails(void) {
    biosim_challenge_spec_t s;
    s.kind = BIOSIM_CHALLENGE_LOCATION_SEQUENCE;
    ctx.agents.challenge_bits[0] = 0;
    TEST_ASSERT_FALSE(biosim_challenge_eval(&s, 0, &ctx).passed);
}

void test_location_sequence_bits_set_passes(void) {
    biosim_challenge_spec_t s;
    s.kind = BIOSIM_CHALLENGE_LOCATION_SEQUENCE;
    ctx.agents.challenge_bits[0] = 5U; /* bits 0 and 2 */
    biosim_challenge_result_t r = biosim_challenge_eval(&s, 0, &ctx);
    TEST_ASSERT_TRUE(r.passed);
    TEST_ASSERT_FLOAT_WITHIN(0.01F, 2.0F / 32.0F, r.score);
}

/* ── location_sequence step hook ─────────────────────────────────────────── */

void test_step_location_sequence_visits_waypoint(void) {
    biosim_challenge_spec_t s;
    s.kind = BIOSIM_CHALLENGE_LOCATION_SEQUENCE;
    /* Install one waypoint at (10, 10); context_free will release it */
    biosim_coord_t *wpt = (biosim_coord_t *)malloc(sizeof(biosim_coord_t));
    TEST_ASSERT_NOT_NULL(wpt);
    wpt[0].x = 10;
    wpt[0].y = 10;
    ctx.barrier_ctrs = wpt;
    ctx.n_barrier_ctrs = 1;

    ctx.agents.loc_x[0] = 10;
    ctx.agents.loc_y[0] = 10;
    ctx.agents.challenge_bits[0] = 0;
    biosim_challenge_step(&s, &ctx, 0, 300);
    TEST_ASSERT_NOT_EQUAL(0U, ctx.agents.challenge_bits[0]);
}

/* ── altruism stub ───────────────────────────────────────────────────────── */

void test_altruism_stub_fails(void) {
    biosim_challenge_spec_t s;
    s.kind = BIOSIM_CHALLENGE_ALTRUISM;
    biosim_challenge_result_t r = biosim_challenge_eval(&s, 0, &ctx);
    TEST_ASSERT_FALSE(r.passed);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, r.score);
}

/* ── Runner ──────────────────────────────────────────────────────────────── */

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_xband_right_half_pass);
    RUN_TEST(test_xband_right_half_fail);
    RUN_TEST(test_xband_mirror_left_pass);
    RUN_TEST(test_xband_mirror_right_pass);
    RUN_TEST(test_xband_mirror_center_fail);
    RUN_TEST(test_disc_center_passes);
    RUN_TEST(test_disc_outside_fails);
    RUN_TEST(test_disc_weighted_center_is_one);
    RUN_TEST(test_disc_weighted_corner_is_less_than_one);
    RUN_TEST(test_corners_at_corner_passes);
    RUN_TEST(test_corners_center_fails);
    RUN_TEST(test_corners_weighted_corner_is_one);
    RUN_TEST(test_corners_weighted_corner_is_less_than_one);
    RUN_TEST(test_against_wall_border_passes);
    RUN_TEST(test_against_wall_interior_fails);
    RUN_TEST(test_migrate_distance_no_move_zero_score);
    RUN_TEST(test_migrate_distance_moved_positive_score);
    RUN_TEST(test_touch_any_wall_bits_zero_fails);
    RUN_TEST(test_touch_any_wall_bits_set_passes);
    RUN_TEST(test_step_touch_any_wall_on_border_sets_bit);
    RUN_TEST(test_step_touch_any_wall_interior_no_bit);
    RUN_TEST(test_radioactive_walls_always_passes_eval);
    RUN_TEST(test_step_radioactive_walls_kills_at_wall);
    RUN_TEST(test_location_sequence_no_bits_fails);
    RUN_TEST(test_location_sequence_bits_set_passes);
    RUN_TEST(test_step_location_sequence_visits_waypoint);
    RUN_TEST(test_altruism_stub_fails);
    return UNITY_END();
}
