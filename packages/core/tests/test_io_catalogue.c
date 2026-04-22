#include "biosim/core/agents.h"
#include "biosim/core/grid.h"
#include "biosim/core/io_catalogue.h"
#include "biosim/core/params.h"
#include "unity.h"

#include <stdint.h>
#include <string.h>

#define CAP    4U
#define GRID_W 16
#define GRID_H 16
#define SIG_SZ ((size_t)GRID_W * (size_t)GRID_H)

static biosim_agents_t agents;
static biosim_grid_t grid;
static uint32_t signal_buf[SIG_SZ];
static biosim_params_t params;

static biosim_sense_ctx_t make_sense(uint32_t idx, uint32_t sim_step) {
    biosim_sense_ctx_t ctx;
    ctx.idx = idx;
    ctx.agents = &agents;
    ctx.grid = &grid;
    ctx.signal = signal_buf;
    ctx.sim_step = sim_step;
    return ctx;
}

static biosim_act_ctx_t make_act(uint32_t idx) {
    biosim_act_ctx_t ctx;
    ctx.idx = idx;
    ctx.agents = &agents;
    ctx.grid = &grid;
    ctx.signal = signal_buf;
    ctx.dx_sum = 0.0F;
    ctx.dy_sum = 0.0F;
    return ctx;
}

void setUp(void) {
    biosim_agents_create(CAP, &agents);
    biosim_grid_create(GRID_W, GRID_H, &grid);
    memset(signal_buf, 0, sizeof(signal_buf));
    biosim_params_init(&params);

    /* place agent 0 at the centre of the grid */
    biosim_coord_t ctr = {GRID_W / 2, GRID_H / 2};
    biosim_agents_init_slot(&agents, 0, ctr, 16, 42ULL);
}

void tearDown(void) {
    biosim_agents_free(&agents);
    biosim_grid_free(&grid);
    biosim_params_free(&params);
}

/* ── LOC_X ──────────────────────────────────────────────────────────────── */

void test_loc_x_left_edge(void) {
    agents.loc_x[0] = 0;
    biosim_sense_ctx_t ctx = make_sense(0, 0);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_LOC_X, &ctx, &params));
}

void test_loc_x_right_edge(void) {
    agents.loc_x[0] = GRID_W - 1;
    biosim_sense_ctx_t ctx = make_sense(0, 0);
    TEST_ASSERT_EQUAL_FLOAT(1.0F, biosim_sensor_eval(BIOSIM_SENSOR_LOC_X, &ctx, &params));
}

void test_loc_x_midpoint(void) {
    agents.loc_x[0] = (GRID_W - 1) / 2;
    biosim_sense_ctx_t ctx = make_sense(0, 0);
    float v = biosim_sensor_eval(BIOSIM_SENSOR_LOC_X, &ctx, &params);
    TEST_ASSERT_FLOAT_WITHIN(0.1F, 0.5F, v);
}

/* ── LOC_Y ──────────────────────────────────────────────────────────────── */

void test_loc_y_top_edge(void) {
    agents.loc_y[0] = 0;
    biosim_sense_ctx_t ctx = make_sense(0, 0);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_LOC_Y, &ctx, &params));
}

void test_loc_y_bottom_edge(void) {
    agents.loc_y[0] = GRID_H - 1;
    biosim_sense_ctx_t ctx = make_sense(0, 0);
    TEST_ASSERT_EQUAL_FLOAT(1.0F, biosim_sensor_eval(BIOSIM_SENSOR_LOC_Y, &ctx, &params));
}

void test_loc_y_midpoint(void) {
    agents.loc_y[0] = (GRID_H - 1) / 2;
    biosim_sense_ctx_t ctx = make_sense(0, 0);
    float v = biosim_sensor_eval(BIOSIM_SENSOR_LOC_Y, &ctx, &params);
    TEST_ASSERT_FLOAT_WITHIN(0.1F, 0.5F, v);
}

/* ── BOUNDARY_DIST_X ────────────────────────────────────────────────────── */

void test_boundary_dist_x_left_edge(void) {
    agents.loc_x[0] = 0;
    biosim_sense_ctx_t ctx = make_sense(0, 0);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_BOUNDARY_DIST_X, &ctx, &params));
}

void test_boundary_dist_x_right_edge(void) {
    agents.loc_x[0] = GRID_W - 1;
    biosim_sense_ctx_t ctx = make_sense(0, 0);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_BOUNDARY_DIST_X, &ctx, &params));
}

void test_boundary_dist_x_center_positive(void) {
    agents.loc_x[0] = GRID_W / 2;
    biosim_sense_ctx_t ctx = make_sense(0, 0);
    float v = biosim_sensor_eval(BIOSIM_SENSOR_BOUNDARY_DIST_X, &ctx, &params);
    TEST_ASSERT_TRUE(v > 0.4F);
}

/* ── BOUNDARY_DIST_Y ────────────────────────────────────────────────────── */

void test_boundary_dist_y_top_edge(void) {
    agents.loc_y[0] = 0;
    biosim_sense_ctx_t ctx = make_sense(0, 0);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_BOUNDARY_DIST_Y, &ctx, &params));
}

void test_boundary_dist_y_bottom_edge(void) {
    agents.loc_y[0] = GRID_H - 1;
    biosim_sense_ctx_t ctx = make_sense(0, 0);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_BOUNDARY_DIST_Y, &ctx, &params));
}

void test_boundary_dist_y_center_positive(void) {
    agents.loc_y[0] = GRID_H / 2;
    biosim_sense_ctx_t ctx = make_sense(0, 0);
    float v = biosim_sensor_eval(BIOSIM_SENSOR_BOUNDARY_DIST_Y, &ctx, &params);
    TEST_ASSERT_TRUE(v > 0.4F);
}

/* ── BOUNDARY_DIST ──────────────────────────────────────────────────────── */

void test_boundary_dist_corner_is_zero(void) {
    agents.loc_x[0] = 0;
    agents.loc_y[0] = 0;
    biosim_sense_ctx_t ctx = make_sense(0, 0);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_BOUNDARY_DIST, &ctx, &params));
}

void test_boundary_dist_center_positive(void) {
    agents.loc_x[0] = GRID_W / 2;
    agents.loc_y[0] = GRID_H / 2;
    biosim_sense_ctx_t ctx = make_sense(0, 0);
    float v = biosim_sensor_eval(BIOSIM_SENSOR_BOUNDARY_DIST, &ctx, &params);
    TEST_ASSERT_TRUE(v > 0.4F);
}

/* ── LAST_MOVE_DIR_X / Y ────────────────────────────────────────────────── */

void test_last_move_dir_x_east(void) {
    agents.last_move_dir[0] = 0; /* E: dx=+1 → (1+1)*0.5 = 1.0 */
    biosim_sense_ctx_t ctx = make_sense(0, 0);
    TEST_ASSERT_EQUAL_FLOAT(1.0F, biosim_sensor_eval(BIOSIM_SENSOR_LAST_MOVE_DIR_X, &ctx, &params));
}

void test_last_move_dir_x_west(void) {
    agents.last_move_dir[0] = 4; /* W: dx=-1 → (-1+1)*0.5 = 0.0 */
    biosim_sense_ctx_t ctx = make_sense(0, 0);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_LAST_MOVE_DIR_X, &ctx, &params));
}

void test_last_move_dir_x_north(void) {
    agents.last_move_dir[0] = 2; /* N: dx=0 → (0+1)*0.5 = 0.5 */
    biosim_sense_ctx_t ctx = make_sense(0, 0);
    TEST_ASSERT_EQUAL_FLOAT(0.5F, biosim_sensor_eval(BIOSIM_SENSOR_LAST_MOVE_DIR_X, &ctx, &params));
}

void test_last_move_dir_y_south(void) {
    agents.last_move_dir[0] = 6; /* S: dy=+1 → (1+1)*0.5 = 1.0 */
    biosim_sense_ctx_t ctx = make_sense(0, 0);
    TEST_ASSERT_EQUAL_FLOAT(1.0F, biosim_sensor_eval(BIOSIM_SENSOR_LAST_MOVE_DIR_Y, &ctx, &params));
}

void test_last_move_dir_y_north(void) {
    agents.last_move_dir[0] = 2; /* N: dy=-1 → (-1+1)*0.5 = 0.0 */
    biosim_sense_ctx_t ctx = make_sense(0, 0);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_LAST_MOVE_DIR_Y, &ctx, &params));
}

void test_last_move_dir_y_east(void) {
    agents.last_move_dir[0] = 0; /* E: dy=0 → (0+1)*0.5 = 0.5 */
    biosim_sense_ctx_t ctx = make_sense(0, 0);
    TEST_ASSERT_EQUAL_FLOAT(0.5F, biosim_sensor_eval(BIOSIM_SENSOR_LAST_MOVE_DIR_Y, &ctx, &params));
}

/* ── OSC1 ───────────────────────────────────────────────────────────────── */

void test_osc1_start_of_cycle(void) {
    /* phase = 0/4 = 0 → (1 - cos(0)) / 2 = 0 */
    agents.osc_period[0] = 4;
    biosim_sense_ctx_t ctx = make_sense(0, 0);
    TEST_ASSERT_FLOAT_WITHIN(1e-5F, 0.0F, biosim_sensor_eval(BIOSIM_SENSOR_OSC1, &ctx, &params));
}

void test_osc1_half_cycle(void) {
    /* phase = 2/4 = 0.5 → (1 - cos(π)) / 2 = 1 */
    agents.osc_period[0] = 4;
    biosim_sense_ctx_t ctx = make_sense(0, 2);
    TEST_ASSERT_FLOAT_WITHIN(1e-5F, 1.0F, biosim_sensor_eval(BIOSIM_SENSOR_OSC1, &ctx, &params));
}

/* ── AGE ────────────────────────────────────────────────────────────────── */

void test_age_zero(void) {
    agents.age[0] = 0;
    biosim_sense_ctx_t ctx = make_sense(0, 0);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_AGE, &ctx, &params));
}

void test_age_max(void) {
    int steps = biosim_params_get_int(&params, "steps-per-gen");
    agents.age[0] = (uint16_t)steps;
    biosim_sense_ctx_t ctx = make_sense(0, 0);
    TEST_ASSERT_FLOAT_WITHIN(1e-5F, 1.0F, biosim_sensor_eval(BIOSIM_SENSOR_AGE, &ctx, &params));
}

/* ── RANDOM ─────────────────────────────────────────────────────────────── */

void test_random_in_range(void) {
    biosim_sense_ctx_t ctx = make_sense(0, 0);
    float v = biosim_sensor_eval(BIOSIM_SENSOR_RANDOM, &ctx, &params);
    TEST_ASSERT_TRUE(v >= 0.0F && v <= 1.0F);
}

void test_random_advances_rng(void) {
    biosim_sense_ctx_t ctx = make_sense(0, 0);
    uint64_t before = agents.rng_state[0];
    biosim_sensor_eval(BIOSIM_SENSOR_RANDOM, &ctx, &params);
    TEST_ASSERT_NOT_EQUAL_UINT64(before, agents.rng_state[0]);
}

/* ── POPULATION ─────────────────────────────────────────────────────────── */

void test_population_empty_neighborhood(void) {
    biosim_grid_zero_fill(&grid);
    biosim_sense_ctx_t ctx = make_sense(0, 0);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_POPULATION, &ctx, &params));
}

void test_population_all_neighbors_occupied(void) {
    /* Fill a 3×3 area around agent with other agents (indices 1..8) so every
     * cell in the default radius-2 neighbourhood is occupied. */
    biosim_grid_zero_fill(&grid);
    int16_t cx = agents.loc_x[0];
    int16_t cy = agents.loc_y[0];
    for (int16_t dy = -1; dy <= 1; dy++) {
        for (int16_t dx = -1; dx <= 1; dx++) {
            biosim_coord_t c = {(int16_t)(cx + dx), (int16_t)(cy + dy)};
            biosim_grid_set(&grid, c, 1U); /* mark as occupied */
        }
    }
    biosim_sense_ctx_t ctx = make_sense(0, 0);
    float v = biosim_sensor_eval(BIOSIM_SENSOR_POPULATION, &ctx, &params);
    TEST_ASSERT_TRUE(v > 0.0F);
}

/* ── SIGNAL0 ────────────────────────────────────────────────────────────── */

void test_signal0_zero(void) {
    memset(signal_buf, 0, sizeof(signal_buf));
    biosim_sense_ctx_t ctx = make_sense(0, 0);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_SIGNAL0, &ctx, &params));
}

void test_signal0_max(void) {
    size_t ci = (size_t)agents.loc_y[0] * GRID_W + (size_t)agents.loc_x[0];
    signal_buf[ci] = 255U;
    biosim_sense_ctx_t ctx = make_sense(0, 0);
    TEST_ASSERT_EQUAL_FLOAT(1.0F, biosim_sensor_eval(BIOSIM_SENSOR_SIGNAL0, &ctx, &params));
}

void test_signal0_midrange(void) {
    size_t ci = (size_t)agents.loc_y[0] * GRID_W + (size_t)agents.loc_x[0];
    signal_buf[ci] = 127U;
    biosim_sense_ctx_t ctx = make_sense(0, 0);
    float v = biosim_sensor_eval(BIOSIM_SENSOR_SIGNAL0, &ctx, &params);
    TEST_ASSERT_FLOAT_WITHIN(0.01F, 127.0F / 255.0F, v);
}

/* ── GENETIC_SIM_FWD ────────────────────────────────────────────────────── */

void test_genetic_sim_fwd_empty_forward(void) {
    /* dir=0 (E): forward cell is (cx+1, cy) — ensure it is empty */
    agents.last_move_dir[0] = 0;
    biosim_coord_t fwd = {(int16_t)(agents.loc_x[0] + 1), agents.loc_y[0]};
    biosim_grid_set(&grid, fwd, BIOSIM_GRID_EMPTY);
    biosim_sense_ctx_t ctx = make_sense(0, 0);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_GENETIC_SIM_FWD, &ctx, &params));
}

void test_genetic_sim_fwd_barrier_forward(void) {
    agents.last_move_dir[0] = 0;
    biosim_coord_t fwd = {(int16_t)(agents.loc_x[0] + 1), agents.loc_y[0]};
    biosim_grid_set(&grid, fwd, BIOSIM_GRID_BARRIER);
    biosim_sense_ctx_t ctx = make_sense(0, 0);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_GENETIC_SIM_FWD, &ctx, &params));
    biosim_grid_set(&grid, fwd, BIOSIM_GRID_EMPTY);
}

void test_genetic_sim_fwd_identical_fingerprint(void) {
    /* Place a neighbour with the same fingerprint one cell to the east. */
    agents.last_move_dir[0] = 0;
    biosim_coord_t fwd = {(int16_t)(agents.loc_x[0] + 1), agents.loc_y[0]};
    biosim_grid_set(&grid, fwd, 2U); /* agent index 1 (1-based) */
    agents.genome_fingerprint[0] = 0xDEADBEEFCAFEBABEULL;
    agents.genome_fingerprint[1] = 0xDEADBEEFCAFEBABEULL;
    biosim_sense_ctx_t ctx = make_sense(0, 0);
    TEST_ASSERT_EQUAL_FLOAT(1.0F, biosim_sensor_eval(BIOSIM_SENSOR_GENETIC_SIM_FWD, &ctx, &params));
    biosim_grid_set(&grid, fwd, BIOSIM_GRID_EMPTY);
}

void test_genetic_sim_fwd_all_bits_differ(void) {
    agents.last_move_dir[0] = 0;
    biosim_coord_t fwd = {(int16_t)(agents.loc_x[0] + 1), agents.loc_y[0]};
    biosim_grid_set(&grid, fwd, 2U);
    agents.genome_fingerprint[0] = 0x0000000000000000ULL;
    agents.genome_fingerprint[1] = 0xFFFFFFFFFFFFFFFFULL;
    biosim_sense_ctx_t ctx = make_sense(0, 0);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_GENETIC_SIM_FWD, &ctx, &params));
    biosim_grid_set(&grid, fwd, BIOSIM_GRID_EMPTY);
}

/* ── placeholder sensors ────────────────────────────────────────────────── */

void test_placeholder_sensors_return_half(void) {
    biosim_sense_ctx_t ctx = make_sense(0, 0);
    TEST_ASSERT_EQUAL_FLOAT(0.5F, biosim_sensor_eval(BIOSIM_SENSOR_POPULATION_FWD, &ctx, &params));
    TEST_ASSERT_EQUAL_FLOAT(0.5F, biosim_sensor_eval(BIOSIM_SENSOR_POPULATION_LR, &ctx, &params));
    TEST_ASSERT_EQUAL_FLOAT(0.5F, biosim_sensor_eval(BIOSIM_SENSOR_BARRIER_FWD, &ctx, &params));
    TEST_ASSERT_EQUAL_FLOAT(0.5F, biosim_sensor_eval(BIOSIM_SENSOR_BARRIER_LR, &ctx, &params));
    TEST_ASSERT_EQUAL_FLOAT(0.5F,
                            biosim_sensor_eval(BIOSIM_SENSOR_LONGPROBE_POP_FWD, &ctx, &params));
    TEST_ASSERT_EQUAL_FLOAT(0.5F,
                            biosim_sensor_eval(BIOSIM_SENSOR_LONGPROBE_BAR_FWD, &ctx, &params));
    TEST_ASSERT_EQUAL_FLOAT(0.5F, biosim_sensor_eval(BIOSIM_SENSOR_SIGNAL0_FWD, &ctx, &params));
    TEST_ASSERT_EQUAL_FLOAT(0.5F, biosim_sensor_eval(BIOSIM_SENSOR_SIGNAL0_LR, &ctx, &params));
}

/* ── SET_RESPONSIVENESS ─────────────────────────────────────────────────── */

void test_set_responsiveness_zero_val(void) {
    /* tanh(0)*0.5 + 0.5 = 0.5 */
    biosim_act_ctx_t ctx = make_act(0);
    biosim_action_apply(BIOSIM_ACTION_SET_RESPONSIVENESS, 0.0F, &ctx, &params);
    TEST_ASSERT_FLOAT_WITHIN(1e-5F, 0.5F, agents.responsiveness[0]);
}

void test_set_responsiveness_positive_val(void) {
    biosim_act_ctx_t ctx = make_act(0);
    biosim_action_apply(BIOSIM_ACTION_SET_RESPONSIVENESS, 1.0F, &ctx, &params);
    float expected = tanhf(1.0F) * 0.5F + 0.5F;
    TEST_ASSERT_FLOAT_WITHIN(1e-5F, expected, agents.responsiveness[0]);
}

/* ── SET_OSCILLATOR_PERIOD ──────────────────────────────────────────────── */

void test_set_oscillator_period_large_positive(void) {
    biosim_act_ctx_t ctx = make_act(0);
    biosim_action_apply(BIOSIM_ACTION_SET_OSCILLATOR_PERIOD, 100.0F, &ctx, &params);
    TEST_ASSERT_TRUE(agents.osc_period[0] <= 2048U);
    TEST_ASSERT_TRUE(agents.osc_period[0] >= 2U);
}

void test_set_oscillator_period_large_negative(void) {
    biosim_act_ctx_t ctx = make_act(0);
    biosim_action_apply(BIOSIM_ACTION_SET_OSCILLATOR_PERIOD, -100.0F, &ctx, &params);
    TEST_ASSERT_TRUE(agents.osc_period[0] >= 2U);
}

/* ── SET_LONGPROBE_DIST ─────────────────────────────────────────────────── */

void test_set_longprobe_dist_large_negative(void) {
    biosim_act_ctx_t ctx = make_act(0);
    biosim_action_apply(BIOSIM_ACTION_SET_LONGPROBE_DIST, -100.0F, &ctx, &params);
    TEST_ASSERT_EQUAL_UINT8(1U, agents.long_probe_dist[0]);
}

void test_set_longprobe_dist_large_positive(void) {
    biosim_act_ctx_t ctx = make_act(0);
    biosim_action_apply(BIOSIM_ACTION_SET_LONGPROBE_DIST, 100.0F, &ctx, &params);
    TEST_ASSERT_EQUAL_UINT8(32U, agents.long_probe_dist[0]);
}

/* ── movement accumulators ──────────────────────────────────────────────── */

void test_move_x_accumulates(void) {
    agents.responsiveness[0] = 1.0F;
    biosim_act_ctx_t ctx = make_act(0);
    biosim_action_apply(BIOSIM_ACTION_MOVE_X, 1.0F, &ctx, &params);
    biosim_action_apply(BIOSIM_ACTION_MOVE_X, 0.5F, &ctx, &params);
    TEST_ASSERT_FLOAT_WITHIN(1e-5F, 1.5F, ctx.dx_sum);
}

void test_move_forward_east_positive_dx(void) {
    agents.last_move_dir[0] = 0; /* E: DIR_DX=+1 */
    agents.responsiveness[0] = 1.0F;
    biosim_act_ctx_t ctx = make_act(0);
    biosim_action_apply(BIOSIM_ACTION_MOVE_FORWARD, 1.0F, &ctx, &params);
    TEST_ASSERT_TRUE(ctx.dx_sum > 0.0F);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, ctx.dy_sum);
}

void test_move_reverse_of_east_is_west(void) {
    agents.last_move_dir[0] = 0; /* E: reverse = W */
    agents.responsiveness[0] = 1.0F;
    biosim_act_ctx_t ctx = make_act(0);
    biosim_action_apply(BIOSIM_ACTION_MOVE_REVERSE, 1.0F, &ctx, &params);
    TEST_ASSERT_TRUE(ctx.dx_sum < 0.0F);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, ctx.dy_sum);
}

void test_move_left_of_east_is_north(void) {
    agents.last_move_dir[0] = 0; /* E: left = N → dy=-1 */
    agents.responsiveness[0] = 1.0F;
    biosim_act_ctx_t ctx = make_act(0);
    biosim_action_apply(BIOSIM_ACTION_MOVE_LEFT, 1.0F, &ctx, &params);
    TEST_ASSERT_TRUE(ctx.dy_sum < 0.0F);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, ctx.dx_sum);
}

void test_move_right_of_east_is_south(void) {
    agents.last_move_dir[0] = 0; /* E: right = S → dy=+1 */
    agents.responsiveness[0] = 1.0F;
    biosim_act_ctx_t ctx = make_act(0);
    biosim_action_apply(BIOSIM_ACTION_MOVE_RIGHT, 1.0F, &ctx, &params);
    TEST_ASSERT_TRUE(ctx.dy_sum > 0.0F);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, ctx.dx_sum);
}

void test_move_forward_north_positive_dy(void) {
    agents.last_move_dir[0] = 2; /* N: DIR_DY=-1 */
    agents.responsiveness[0] = 1.0F;
    biosim_act_ctx_t ctx = make_act(0);
    biosim_action_apply(BIOSIM_ACTION_MOVE_FORWARD, 1.0F, &ctx, &params);
    TEST_ASSERT_TRUE(ctx.dy_sum < 0.0F);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, ctx.dx_sum);
}

void test_move_reverse_of_north_is_south(void) {
    agents.last_move_dir[0] = 2; /* N: reverse = S */
    agents.responsiveness[0] = 1.0F;
    biosim_act_ctx_t ctx = make_act(0);
    biosim_action_apply(BIOSIM_ACTION_MOVE_REVERSE, 1.0F, &ctx, &params);
    TEST_ASSERT_TRUE(ctx.dy_sum > 0.0F);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, ctx.dx_sum);
}

void test_move_left_of_north_is_west(void) {
    agents.last_move_dir[0] = 2; /* N: left = W → dx=-1 */
    agents.responsiveness[0] = 1.0F;
    biosim_act_ctx_t ctx = make_act(0);
    biosim_action_apply(BIOSIM_ACTION_MOVE_LEFT, 1.0F, &ctx, &params);
    TEST_ASSERT_TRUE(ctx.dx_sum < 0.0F);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, ctx.dy_sum);
}

void test_move_right_of_north_is_east(void) {
    agents.last_move_dir[0] = 2; /* N: right = E → dx=+1 */
    agents.responsiveness[0] = 1.0F;
    biosim_act_ctx_t ctx = make_act(0);
    biosim_action_apply(BIOSIM_ACTION_MOVE_RIGHT, 1.0F, &ctx, &params);
    TEST_ASSERT_TRUE(ctx.dx_sum > 0.0F);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, ctx.dy_sum);
}

/* ── cardinal movement accumulators ──────────────────────-──────────────── */

void test_move_east_cardinal(void) {
    agents.responsiveness[0] = 1.0F;
    biosim_act_ctx_t ctx = make_act(0);
    biosim_action_apply(BIOSIM_ACTION_MOVE_EAST, 0.0F, &ctx, &params);
    TEST_ASSERT_TRUE(ctx.dx_sum > 0.0F);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, ctx.dy_sum);
}

void test_move_west_cardinal(void) {
    agents.responsiveness[0] = 1.0F;
    biosim_act_ctx_t ctx = make_act(0);
    biosim_action_apply(BIOSIM_ACTION_MOVE_WEST, 0.0F, &ctx, &params);
    TEST_ASSERT_TRUE(ctx.dx_sum < 0.0F);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, ctx.dy_sum);
}

void test_move_north_cardinal(void) {
    agents.responsiveness[0] = 1.0F;
    biosim_act_ctx_t ctx = make_act(0);
    biosim_action_apply(BIOSIM_ACTION_MOVE_NORTH, 0.0F, &ctx, &params);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, ctx.dx_sum);
    TEST_ASSERT_TRUE(ctx.dy_sum < 0.0F);
}

void test_move_south_cardinal(void) {
    agents.responsiveness[0] = 1.0F;
    biosim_act_ctx_t ctx = make_act(0);
    biosim_action_apply(BIOSIM_ACTION_MOVE_SOUTH, 0.0F, &ctx, &params);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, ctx.dx_sum);
    TEST_ASSERT_TRUE(ctx.dy_sum > 0.0F);
}

/* ── EMIT_SIGNAL0 ───────────────────────────────────────────────────────── */

void test_emit_signal0_below_threshold(void) {
    memset(signal_buf, 0, sizeof(signal_buf));
    biosim_act_ctx_t ctx = make_act(0);
    biosim_action_apply(BIOSIM_ACTION_EMIT_SIGNAL0, 0.0F, &ctx, &params);
    size_t ci = (size_t)agents.loc_y[0] * GRID_W + (size_t)agents.loc_x[0];
    TEST_ASSERT_EQUAL_UINT32(0U, signal_buf[ci]);
}

void test_emit_signal0_center_plus_two(void) {
    memset(signal_buf, 0, sizeof(signal_buf));
    biosim_act_ctx_t ctx = make_act(0);
    biosim_action_apply(BIOSIM_ACTION_EMIT_SIGNAL0, 1.0F, &ctx, &params);
    size_t ci = (size_t)agents.loc_y[0] * GRID_W + (size_t)agents.loc_x[0];
    TEST_ASSERT_EQUAL_UINT32(2U, signal_buf[ci]);
}

void test_emit_signal0_clamped_at_255(void) {
    size_t ci = (size_t)agents.loc_y[0] * GRID_W + (size_t)agents.loc_x[0];
    signal_buf[ci] = 254U;
    biosim_act_ctx_t ctx = make_act(0);
    biosim_action_apply(BIOSIM_ACTION_EMIT_SIGNAL0, 1.0F, &ctx, &params);
    TEST_ASSERT_EQUAL_UINT32(255U, signal_buf[ci]);
}

/* ── KILL_FORWARD ───────────────────────────────────────────────────────── */

void test_kill_forward_below_threshold(void) {
    agents.last_move_dir[0] = 0;
    biosim_coord_t fwd = {(int16_t)(agents.loc_x[0] + 1), agents.loc_y[0]};
    biosim_grid_set(&grid, fwd, 2U); /* agent 1 */
    biosim_agents_init_slot(&agents, 1, fwd, 16, 99ULL);

    biosim_act_ctx_t ctx = make_act(0);
    biosim_action_apply(BIOSIM_ACTION_KILL_FORWARD, 0.0F, &ctx, &params);
    TEST_ASSERT_EQUAL_UINT8(1U, agents.alive[1]);

    biosim_grid_set(&grid, fwd, BIOSIM_GRID_EMPTY);
}

void test_kill_forward_above_threshold(void) {
    agents.last_move_dir[0] = 0;
    biosim_coord_t fwd = {(int16_t)(agents.loc_x[0] + 1), agents.loc_y[0]};
    biosim_grid_set(&grid, fwd, 2U); /* agent 1 */
    biosim_agents_init_slot(&agents, 1, fwd, 16, 99ULL);

    biosim_act_ctx_t ctx = make_act(0);
    biosim_action_apply(BIOSIM_ACTION_KILL_FORWARD, 1.0F, &ctx, &params);
    TEST_ASSERT_EQUAL_UINT8(0U, agents.alive[1]);

    biosim_grid_set(&grid, fwd, BIOSIM_GRID_EMPTY);
}

void test_kill_forward_empty_cell_no_crash(void) {
    agents.last_move_dir[0] = 0;
    biosim_coord_t fwd = {(int16_t)(agents.loc_x[0] + 1), agents.loc_y[0]};
    biosim_grid_set(&grid, fwd, BIOSIM_GRID_EMPTY);
    biosim_act_ctx_t ctx = make_act(0);
    biosim_action_apply(BIOSIM_ACTION_KILL_FORWARD, 1.0F, &ctx, &params);
    /* No assertion needed — just verifying no crash. */
}

/* ── finalize movement ──────────────────────────────────────────────────── */

void test_finalize_no_accumulation(void) {
    biosim_act_ctx_t ctx = make_act(0);
    ctx.dx_sum = 0.0F;
    ctx.dy_sum = 0.0F;
    biosim_action_finalize_movement(&ctx, &params);
    /* With zero sums tanh gives 0, so the step probability is 0; desired == loc. */
    TEST_ASSERT_EQUAL_INT16(agents.loc_x[0], agents.desired_x[0]);
    TEST_ASSERT_EQUAL_INT16(agents.loc_y[0], agents.desired_y[0]);
}

void test_finalize_boundary_clamp_east(void) {
    agents.loc_x[0] = GRID_W - 1;
    biosim_act_ctx_t ctx = make_act(0);
    ctx.dx_sum = 100.0F;
    ctx.dy_sum = 0.0F;
    biosim_action_finalize_movement(&ctx, &params);
    TEST_ASSERT_TRUE(agents.desired_x[0] < GRID_W);
}

void test_finalize_boundary_clamp_west(void) {
    agents.loc_x[0] = 0;
    biosim_act_ctx_t ctx = make_act(0);
    ctx.dx_sum = -100.0F; /* strong westward push */
    ctx.dy_sum = 0.0F;
    biosim_action_finalize_movement(&ctx, &params);
    TEST_ASSERT_TRUE(agents.desired_x[0] >= 0);
}

void test_finalize_boundary_clamp_north(void) {
    agents.loc_y[0] = 0;
    biosim_act_ctx_t ctx = make_act(0);
    ctx.dx_sum = 0.0F;
    ctx.dy_sum = -100.0F; /* strong northward push */
    biosim_action_finalize_movement(&ctx, &params);
    TEST_ASSERT_TRUE(agents.desired_y[0] >= 0);
}

void test_finalize_boundary_clamp_south(void) {
    agents.loc_y[0] = GRID_H - 1;
    biosim_act_ctx_t ctx = make_act(0);
    ctx.dx_sum = 0.0F;
    ctx.dy_sum = 100.0F; /* strong southward push */
    biosim_action_finalize_movement(&ctx, &params);
    TEST_ASSERT_TRUE(agents.desired_y[0] < GRID_H);
}

void test_finalize_strong_east_steps_one_cell(void) {
    /* Large dx_sum: tanh(100*0.5)≈1.0 > any rng_float draw → step is certain. */
    agents.loc_x[0] = GRID_W / 2;
    agents.loc_y[0] = GRID_H / 2;
    int16_t start_x = agents.loc_x[0];
    int16_t start_y = agents.loc_y[0];
    biosim_act_ctx_t ctx = make_act(0);
    ctx.dx_sum = 100.0F;
    ctx.dy_sum = 0.0F;
    biosim_action_finalize_movement(&ctx, &params);
    TEST_ASSERT_EQUAL_INT16(start_x + 1, agents.desired_x[0]);
    TEST_ASSERT_EQUAL_INT16(start_y, agents.desired_y[0]);
}

void test_finalize_strong_north_steps_one_cell(void) {
    /* Large negative dy_sum → certain northward step (decreasing y). */
    agents.loc_x[0] = GRID_W / 2;
    agents.loc_y[0] = GRID_H / 2;
    int16_t start_x = agents.loc_x[0];
    int16_t start_y = agents.loc_y[0];
    biosim_act_ctx_t ctx = make_act(0);
    ctx.dx_sum = 0.0F;
    ctx.dy_sum = -100.0F;
    biosim_action_finalize_movement(&ctx, &params);
    TEST_ASSERT_EQUAL_INT16(start_x, agents.desired_x[0]);
    TEST_ASSERT_EQUAL_INT16(start_y - 1, agents.desired_y[0]);
}

/* ── Runner ─────────────────────────────────────────────────────────────── */

int main(void) {
    UNITY_BEGIN();
    /* LOC_X */
    RUN_TEST(test_loc_x_left_edge);
    RUN_TEST(test_loc_x_right_edge);
    RUN_TEST(test_loc_x_midpoint);
    /* LOC_Y */
    RUN_TEST(test_loc_y_top_edge);
    RUN_TEST(test_loc_y_bottom_edge);
    RUN_TEST(test_loc_y_midpoint);
    /* BOUNDARY_DIST_X */
    RUN_TEST(test_boundary_dist_x_left_edge);
    RUN_TEST(test_boundary_dist_x_right_edge);
    RUN_TEST(test_boundary_dist_x_center_positive);
    /* BOUNDARY_DIST_Y */
    RUN_TEST(test_boundary_dist_y_top_edge);
    RUN_TEST(test_boundary_dist_y_bottom_edge);
    RUN_TEST(test_boundary_dist_y_center_positive);
    /* BOUNDARY_DIST */
    RUN_TEST(test_boundary_dist_corner_is_zero);
    RUN_TEST(test_boundary_dist_center_positive);
    /* LAST_MOVE_DIR */
    RUN_TEST(test_last_move_dir_x_east);
    RUN_TEST(test_last_move_dir_x_west);
    RUN_TEST(test_last_move_dir_x_north);
    RUN_TEST(test_last_move_dir_y_south);
    RUN_TEST(test_last_move_dir_y_north);
    RUN_TEST(test_last_move_dir_y_east);
    /* OSC1 */
    RUN_TEST(test_osc1_start_of_cycle);
    RUN_TEST(test_osc1_half_cycle);
    /* AGE */
    RUN_TEST(test_age_zero);
    RUN_TEST(test_age_max);
    /* RANDOM */
    RUN_TEST(test_random_in_range);
    RUN_TEST(test_random_advances_rng);
    /* POPULATION */
    RUN_TEST(test_population_empty_neighborhood);
    RUN_TEST(test_population_all_neighbors_occupied);
    /* SIGNAL0 */
    RUN_TEST(test_signal0_zero);
    RUN_TEST(test_signal0_max);
    RUN_TEST(test_signal0_midrange);
    /* GENETIC_SIM_FWD */
    RUN_TEST(test_genetic_sim_fwd_empty_forward);
    RUN_TEST(test_genetic_sim_fwd_barrier_forward);
    RUN_TEST(test_genetic_sim_fwd_identical_fingerprint);
    RUN_TEST(test_genetic_sim_fwd_all_bits_differ);
    /* placeholders */
    RUN_TEST(test_placeholder_sensors_return_half);
    /* SET_RESPONSIVENESS */
    RUN_TEST(test_set_responsiveness_zero_val);
    RUN_TEST(test_set_responsiveness_positive_val);
    /* SET_OSCILLATOR_PERIOD */
    RUN_TEST(test_set_oscillator_period_large_positive);
    RUN_TEST(test_set_oscillator_period_large_negative);
    /* SET_LONGPROBE_DIST */
    RUN_TEST(test_set_longprobe_dist_large_negative);
    RUN_TEST(test_set_longprobe_dist_large_positive);
    /* movement accumulators */
    RUN_TEST(test_move_x_accumulates);
    RUN_TEST(test_move_forward_east_positive_dx);
    RUN_TEST(test_move_reverse_of_east_is_west);
    RUN_TEST(test_move_left_of_east_is_north);
    RUN_TEST(test_move_right_of_east_is_south);
    RUN_TEST(test_move_forward_north_positive_dy);
    RUN_TEST(test_move_reverse_of_north_is_south);
    RUN_TEST(test_move_left_of_north_is_west);
    RUN_TEST(test_move_right_of_north_is_east);
    /* cardinal movement accumulators */
    RUN_TEST(test_move_east_cardinal);
    RUN_TEST(test_move_west_cardinal);
    RUN_TEST(test_move_north_cardinal);
    RUN_TEST(test_move_south_cardinal);
    /* EMIT_SIGNAL0 */
    RUN_TEST(test_emit_signal0_below_threshold);
    RUN_TEST(test_emit_signal0_center_plus_two);
    RUN_TEST(test_emit_signal0_clamped_at_255);
    /* KILL_FORWARD */
    RUN_TEST(test_kill_forward_below_threshold);
    RUN_TEST(test_kill_forward_above_threshold);
    RUN_TEST(test_kill_forward_empty_cell_no_crash);
    /* finalize movement */
    RUN_TEST(test_finalize_no_accumulation);
    RUN_TEST(test_finalize_boundary_clamp_east);
    RUN_TEST(test_finalize_boundary_clamp_west);
    RUN_TEST(test_finalize_boundary_clamp_north);
    RUN_TEST(test_finalize_boundary_clamp_south);
    RUN_TEST(test_finalize_strong_east_steps_one_cell);
    RUN_TEST(test_finalize_strong_north_steps_one_cell);
    return UNITY_END();
}
