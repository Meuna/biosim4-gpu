#include "biosim/core/grid.h"
#include "biosim/core/io_eval.h"
#include "biosim/core/sim.h"
#include "unity.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define CAP    4U
#define GRID_W 16
#define GRID_H 16

static biosim_sim_t sim;

void setUp(void) {
    memset(&sim, 0, sizeof(sim));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_agents_create(CAP, &sim.agents));
    TEST_ASSERT_EQUAL_INT(BIOSIM_OK, biosim_grid_create(GRID_W, GRID_H, &sim.grid));
    sim.signal_len = (size_t)GRID_W * (size_t)GRID_H;
    sim.signal = calloc(sim.signal_len, sizeof(uint32_t));
    sim.steps_per_gen = 300;
    sim.sensor_radius = 2;

    /* place agent 0 at the centre of the grid */
    biosim_coord_t ctr = {GRID_W / 2, GRID_H / 2};
    biosim_agents_init_slot(&sim.agents, 0, ctr, 16, 42ULL);
}

void tearDown(void) {
    biosim_sim_free(&sim);
}

/* ── LOC_X ──────────────────────────────────────────────────────────────── */

void test_loc_x_left_edge(void) {
    sim.agents.loc_x[0] = 0;
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_LOC_X, 0, &sim));
}

void test_loc_x_right_edge(void) {
    sim.agents.loc_x[0] = GRID_W - 1;
    TEST_ASSERT_EQUAL_FLOAT(1.0F, biosim_sensor_eval(BIOSIM_SENSOR_LOC_X, 0, &sim));
}

void test_loc_x_midpoint(void) {
    sim.agents.loc_x[0] = (GRID_W - 1) / 2;
    float v = biosim_sensor_eval(BIOSIM_SENSOR_LOC_X, 0, &sim);
    TEST_ASSERT_FLOAT_WITHIN(0.1F, 0.5F, v);
}

/* ── LOC_Y ──────────────────────────────────────────────────────────────── */

void test_loc_y_top_edge(void) {
    sim.agents.loc_y[0] = 0;
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_LOC_Y, 0, &sim));
}

void test_loc_y_bottom_edge(void) {
    sim.agents.loc_y[0] = GRID_H - 1;
    TEST_ASSERT_EQUAL_FLOAT(1.0F, biosim_sensor_eval(BIOSIM_SENSOR_LOC_Y, 0, &sim));
}

void test_loc_y_midpoint(void) {
    sim.agents.loc_y[0] = (GRID_H - 1) / 2;
    float v = biosim_sensor_eval(BIOSIM_SENSOR_LOC_Y, 0, &sim);
    TEST_ASSERT_FLOAT_WITHIN(0.1F, 0.5F, v);
}

/* ── BOUNDARY_DIST_X ────────────────────────────────────────────────────── */

void test_boundary_dist_x_left_edge(void) {
    sim.agents.loc_x[0] = 0;
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_BOUNDARY_DIST_X, 0, &sim));
}

void test_boundary_dist_x_right_edge(void) {
    sim.agents.loc_x[0] = GRID_W - 1;
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_BOUNDARY_DIST_X, 0, &sim));
}

void test_boundary_dist_x_center_positive(void) {
    sim.agents.loc_x[0] = GRID_W / 2;
    float v = biosim_sensor_eval(BIOSIM_SENSOR_BOUNDARY_DIST_X, 0, &sim);
    TEST_ASSERT_TRUE(v > 0.4F);
}

/* ── BOUNDARY_DIST_Y ────────────────────────────────────────────────────── */

void test_boundary_dist_y_top_edge(void) {
    sim.agents.loc_y[0] = 0;
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_BOUNDARY_DIST_Y, 0, &sim));
}

void test_boundary_dist_y_bottom_edge(void) {
    sim.agents.loc_y[0] = GRID_H - 1;
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_BOUNDARY_DIST_Y, 0, &sim));
}

void test_boundary_dist_y_center_positive(void) {
    sim.agents.loc_y[0] = GRID_H / 2;
    float v = biosim_sensor_eval(BIOSIM_SENSOR_BOUNDARY_DIST_Y, 0, &sim);
    TEST_ASSERT_TRUE(v > 0.4F);
}

/* ── BOUNDARY_DIST ──────────────────────────────────────────────────────── */

void test_boundary_dist_corner_is_zero(void) {
    sim.agents.loc_x[0] = 0;
    sim.agents.loc_y[0] = 0;
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_BOUNDARY_DIST, 0, &sim));
}

void test_boundary_dist_center_positive(void) {
    sim.agents.loc_x[0] = GRID_W / 2;
    sim.agents.loc_y[0] = GRID_H / 2;
    float v = biosim_sensor_eval(BIOSIM_SENSOR_BOUNDARY_DIST, 0, &sim);
    TEST_ASSERT_TRUE(v > 0.4F);
}

/* ── LAST_MOVE_DIR_X / Y ────────────────────────────────────────────────── */

void test_last_move_dir_x_east(void) {
    sim.agents.last_move_dir[0] = 0; /* E: dx=+1 → (1+1)*0.5 = 1.0 */
    TEST_ASSERT_EQUAL_FLOAT(1.0F, biosim_sensor_eval(BIOSIM_SENSOR_LAST_MOVE_DIR_X, 0, &sim));
}

void test_last_move_dir_x_west(void) {
    sim.agents.last_move_dir[0] = 4; /* W: dx=-1 → (-1+1)*0.5 = 0.0 */
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_LAST_MOVE_DIR_X, 0, &sim));
}

void test_last_move_dir_x_north(void) {
    sim.agents.last_move_dir[0] = 2; /* N: dx=0 → (0+1)*0.5 = 0.5 */
    TEST_ASSERT_EQUAL_FLOAT(0.5F, biosim_sensor_eval(BIOSIM_SENSOR_LAST_MOVE_DIR_X, 0, &sim));
}

void test_last_move_dir_y_south(void) {
    sim.agents.last_move_dir[0] = 6; /* S: dy=+1 → (1+1)*0.5 = 1.0 */
    TEST_ASSERT_EQUAL_FLOAT(1.0F, biosim_sensor_eval(BIOSIM_SENSOR_LAST_MOVE_DIR_Y, 0, &sim));
}

void test_last_move_dir_y_north(void) {
    sim.agents.last_move_dir[0] = 2; /* N: dy=-1 → (-1+1)*0.5 = 0.0 */
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_LAST_MOVE_DIR_Y, 0, &sim));
}

void test_last_move_dir_y_east(void) {
    sim.agents.last_move_dir[0] = 0; /* E: dy=0 → (0+1)*0.5 = 0.5 */
    TEST_ASSERT_EQUAL_FLOAT(0.5F, biosim_sensor_eval(BIOSIM_SENSOR_LAST_MOVE_DIR_Y, 0, &sim));
}

/* ── OSC1 ───────────────────────────────────────────────────────────────── */

void test_osc1_start_of_cycle(void) {
    /* phase = 0/4 = 0 → (1 - cos(0)) / 2 = 0 */
    sim.agents.osc_period[0] = 4;
    TEST_ASSERT_FLOAT_WITHIN(1e-5F, 0.0F, biosim_sensor_eval(BIOSIM_SENSOR_OSC1, 0, &sim));
}

void test_osc1_half_cycle(void) {
    /* phase = 2/4 = 0.5 → (1 - cos(π)) / 2 = 1 */
    sim.agents.osc_period[0] = 4;
    sim.step = 2;
    TEST_ASSERT_FLOAT_WITHIN(1e-5F, 1.0F, biosim_sensor_eval(BIOSIM_SENSOR_OSC1, 0, &sim));
}

/* ── AGE ────────────────────────────────────────────────────────────────── */

void test_age_zero(void) {
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_AGE, 0, &sim));
}

void test_age_max(void) {
    sim.step = sim.steps_per_gen;
    TEST_ASSERT_FLOAT_WITHIN(1e-5F, 1.0F, biosim_sensor_eval(BIOSIM_SENSOR_AGE, 0, &sim));
}

/* ── RANDOM ─────────────────────────────────────────────────────────────── */

void test_random_in_range(void) {
    float v = biosim_sensor_eval(BIOSIM_SENSOR_RANDOM, 0, &sim);
    TEST_ASSERT_TRUE(v >= 0.0F && v <= 1.0F);
}

void test_random_advances_rng(void) {
    uint64_t before = sim.agents.rng_state[0];
    biosim_sensor_eval(BIOSIM_SENSOR_RANDOM, 0, &sim);
    TEST_ASSERT_NOT_EQUAL_UINT64(before, sim.agents.rng_state[0]);
}

/* ── POPULATION ─────────────────────────────────────────────────────────── */

void test_population_empty_neighborhood(void) {
    biosim_grid_zero_fill(&sim.grid);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_POPULATION, 0, &sim));
}

void test_population_all_neighbors_occupied(void) {
    /* Fill a 3×3 area around agent with other agents (indices 1..8) so every
     * cell in the default radius-2 neighbourhood is occupied. */
    biosim_grid_zero_fill(&sim.grid);
    int32_t x = sim.agents.loc_x[0];
    int32_t y = sim.agents.loc_y[0];
    for (int32_t dy = -1; dy <= 1; dy++) {
        for (int32_t dx = -1; dx <= 1; dx++) {
            biosim_coord_t c = {x + dx, y + dy};
            biosim_grid_set(&sim.grid, c, 1U); /* mark as occupied */
        }
    }
    float v = biosim_sensor_eval(BIOSIM_SENSOR_POPULATION, 0, &sim);
    TEST_ASSERT_TRUE(v > 0.0F);
}

/* ── SIGNAL0 ────────────────────────────────────────────────────────────── */

void test_signal0_zero(void) {
    memset(sim.signal, 0, sim.signal_len * sizeof(uint32_t));
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_SIGNAL0, 0, &sim));
}

void test_signal0_max(void) {
    size_t ci = (size_t)sim.agents.loc_y[0] * GRID_W + (size_t)sim.agents.loc_x[0];
    sim.signal[ci] = 255U;
    TEST_ASSERT_EQUAL_FLOAT(1.0F, biosim_sensor_eval(BIOSIM_SENSOR_SIGNAL0, 0, &sim));
}

void test_signal0_midrange(void) {
    size_t ci = (size_t)sim.agents.loc_y[0] * GRID_W + (size_t)sim.agents.loc_x[0];
    sim.signal[ci] = 127U;
    float v = biosim_sensor_eval(BIOSIM_SENSOR_SIGNAL0, 0, &sim);
    TEST_ASSERT_FLOAT_WITHIN(0.01F, 127.0F / 255.0F, v);
}

/* ── SIGNAL0_FWD ────────────────────────────────────────────────────────── */

void test_signal0_fwd_empty(void) {
    /* all signal zeros → front_sum=0, rear_sum=0 → 0.5 */
    sim.agents.last_move_dir[0] = 0;
    memset(sim.signal, 0, sim.signal_len * sizeof(uint32_t));
    TEST_ASSERT_EQUAL_FLOAT(0.5F, biosim_sensor_eval(BIOSIM_SENSOR_SIGNAL0_FWD, 0, &sim));
}

void test_signal0_fwd_all_max(void) {
    /* dir=0: all 4 front cells at 255 → front_sum=1020, rear_sum=0 → 1.0 */
    sim.agents.last_move_dir[0] = 0;
    memset(sim.signal, 0, sim.signal_len * sizeof(uint32_t));
    int32_t x = sim.agents.loc_x[0];
    int32_t y = sim.agents.loc_y[0];
    sim.signal[(size_t)(y - 1) * GRID_W + (size_t)(x + 1)] = 255U;
    sim.signal[(size_t)y * GRID_W + (size_t)(x + 1)] = 255U;
    sim.signal[(size_t)(y + 1) * GRID_W + (size_t)(x + 1)] = 255U;
    sim.signal[(size_t)y * GRID_W + (size_t)(x + 2)] = 255U;
    TEST_ASSERT_EQUAL_FLOAT(1.0F, biosim_sensor_eval(BIOSIM_SENSOR_SIGNAL0_FWD, 0, &sim));
    memset(sim.signal, 0, sim.signal_len * sizeof(uint32_t));
}

void test_signal0_fwd_backward_only(void) {
    /* dir=0: signal only in rear half → front_sum=0, rear_sum>0 → 0.0 */
    sim.agents.last_move_dir[0] = 0;
    memset(sim.signal, 0, sim.signal_len * sizeof(uint32_t));
    int32_t x = sim.agents.loc_x[0];
    int32_t y = sim.agents.loc_y[0];
    sim.signal[(size_t)y * GRID_W + (size_t)(x - 2)] = 255U;
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_SIGNAL0_FWD, 0, &sim));
    memset(sim.signal, 0, sim.signal_len * sizeof(uint32_t));
}

void test_signal0_fwd_symmetric(void) {
    /* dir=0: equal signal front and rear → 0.5 */
    sim.agents.last_move_dir[0] = 0;
    memset(sim.signal, 0, sim.signal_len * sizeof(uint32_t));
    int32_t x = sim.agents.loc_x[0];
    int32_t y = sim.agents.loc_y[0];
    sim.signal[(size_t)y * GRID_W + (size_t)(x + 1)] = 200U;
    sim.signal[(size_t)y * GRID_W + (size_t)(x - 1)] = 200U;
    TEST_ASSERT_EQUAL_FLOAT(0.5F, biosim_sensor_eval(BIOSIM_SENSOR_SIGNAL0_FWD, 0, &sim));
    memset(sim.signal, 0, sim.signal_len * sizeof(uint32_t));
}

void test_signal0_fwd_over_255(void) {
    /* dir=0: front cell at 300 → clamped to 255 → front_sum=255, rear_sum=0 → 1.0 */
    sim.agents.last_move_dir[0] = 0;
    memset(sim.signal, 0, sim.signal_len * sizeof(uint32_t));
    int32_t x = sim.agents.loc_x[0];
    int32_t y = sim.agents.loc_y[0];
    sim.signal[(size_t)y * GRID_W + (size_t)(x + 1)] = 300U;
    TEST_ASSERT_EQUAL_FLOAT(1.0F, biosim_sensor_eval(BIOSIM_SENSOR_SIGNAL0_FWD, 0, &sim));
    memset(sim.signal, 0, sim.signal_len * sizeof(uint32_t));
}

/* ── SIGNAL0_LR ──────────────────────────────────────────────────────────── */
/* dir=0 (East): fwd_x=1, fwd_y=0; lateral = dx*0 - dy*1 = -dy.
 * LEFT (lateral>0): dy<0 cells — e.g. (8,7), (7,7), (9,7), (8,6).
 * RIGHT (lateral<0): dy>0 cells — e.g. (8,9), (7,9), (9,9), (8,10). */

void test_signal0_lr_empty(void) {
    /* all zeros → L=0, R=0 → 0.0 */
    sim.agents.last_move_dir[0] = 0;
    memset(sim.signal, 0, sim.signal_len * sizeof(uint32_t));
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_SIGNAL0_LR, 0, &sim));
}

void test_signal0_lr_left_only(void) {
    /* signal 255 at left cell (8,7) only → L=255, R=0 → 1.0 */
    sim.agents.last_move_dir[0] = 0;
    memset(sim.signal, 0, sim.signal_len * sizeof(uint32_t));
    int32_t x = sim.agents.loc_x[0];
    int32_t y = sim.agents.loc_y[0];
    sim.signal[(size_t)(y - 1) * GRID_W + (size_t)x] = 255U;
    TEST_ASSERT_EQUAL_FLOAT(1.0F, biosim_sensor_eval(BIOSIM_SENSOR_SIGNAL0_LR, 0, &sim));
    memset(sim.signal, 0, sim.signal_len * sizeof(uint32_t));
}

void test_signal0_lr_right_only(void) {
    /* signal 255 at right cell (8,9) only → L=0, R=255 → -1.0 */
    sim.agents.last_move_dir[0] = 0;
    memset(sim.signal, 0, sim.signal_len * sizeof(uint32_t));
    int32_t x = sim.agents.loc_x[0];
    int32_t y = sim.agents.loc_y[0];
    sim.signal[(size_t)(y + 1) * GRID_W + (size_t)x] = 255U;
    TEST_ASSERT_EQUAL_FLOAT(-1.0F, biosim_sensor_eval(BIOSIM_SENSOR_SIGNAL0_LR, 0, &sim));
    memset(sim.signal, 0, sim.signal_len * sizeof(uint32_t));
}

void test_signal0_lr_balanced(void) {
    /* equal signal on both sides → (L-R)/(L+R) = 0.0 */
    sim.agents.last_move_dir[0] = 0;
    memset(sim.signal, 0, sim.signal_len * sizeof(uint32_t));
    int32_t x = sim.agents.loc_x[0];
    int32_t y = sim.agents.loc_y[0];
    sim.signal[(size_t)(y - 1) * GRID_W + (size_t)x] = 100U;
    sim.signal[(size_t)(y + 1) * GRID_W + (size_t)x] = 100U;
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_SIGNAL0_LR, 0, &sim));
    memset(sim.signal, 0, sim.signal_len * sizeof(uint32_t));
}

void test_signal0_lr_asymmetric(void) {
    /* L=200, R=100 → (200-100)/(200+100) = 1/3 */
    sim.agents.last_move_dir[0] = 0;
    memset(sim.signal, 0, sim.signal_len * sizeof(uint32_t));
    int32_t x = sim.agents.loc_x[0];
    int32_t y = sim.agents.loc_y[0];
    sim.signal[(size_t)(y - 1) * GRID_W + (size_t)x] = 200U;
    sim.signal[(size_t)(y + 1) * GRID_W + (size_t)x] = 100U;
    TEST_ASSERT_FLOAT_WITHIN(
        1e-5F, 1.0F / 3.0F, biosim_sensor_eval(BIOSIM_SENSOR_SIGNAL0_LR, 0, &sim)
    );
    memset(sim.signal, 0, sim.signal_len * sizeof(uint32_t));
}

/* ── GENETIC_SIM_FWD ────────────────────────────────────────────────────── */

void test_genetic_sim_fwd_empty_forward(void) {
    /* dir=0 (E): forward cell is (x+1, y) — ensure it is empty */
    sim.agents.last_move_dir[0] = 0;
    biosim_coord_t fwd = {sim.agents.loc_x[0] + 1, sim.agents.loc_y[0]};
    biosim_grid_set(&sim.grid, fwd, BIOSIM_GRID_EMPTY);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_GENETIC_SIM_FWD, 0, &sim));
}

void test_genetic_sim_fwd_barrier_forward(void) {
    sim.agents.last_move_dir[0] = 0;
    biosim_coord_t fwd = {sim.agents.loc_x[0] + 1, sim.agents.loc_y[0]};
    biosim_grid_set(&sim.grid, fwd, BIOSIM_GRID_BARRIER);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_GENETIC_SIM_FWD, 0, &sim));
    biosim_grid_set(&sim.grid, fwd, BIOSIM_GRID_EMPTY);
}

void test_genetic_sim_fwd_identical_fingerprint(void) {
    /* Place a neighbour with the same fingerprint one cell to the east. */
    sim.agents.last_move_dir[0] = 0;
    biosim_coord_t fwd = {sim.agents.loc_x[0] + 1, sim.agents.loc_y[0]};
    biosim_grid_set(&sim.grid, fwd, 2U); /* agent index 1 (1-based) */
    sim.agents.genome_fingerprint[0] = 0xDEADBEEFCAFEBABEULL;
    sim.agents.genome_fingerprint[1] = 0xDEADBEEFCAFEBABEULL;
    TEST_ASSERT_EQUAL_FLOAT(1.0F, biosim_sensor_eval(BIOSIM_SENSOR_GENETIC_SIM_FWD, 0, &sim));
    biosim_grid_set(&sim.grid, fwd, BIOSIM_GRID_EMPTY);
}

void test_genetic_sim_fwd_all_bits_differ(void) {
    sim.agents.last_move_dir[0] = 0;
    biosim_coord_t fwd = {sim.agents.loc_x[0] + 1, sim.agents.loc_y[0]};
    biosim_grid_set(&sim.grid, fwd, 2U);
    sim.agents.genome_fingerprint[0] = 0x0000000000000000ULL;
    sim.agents.genome_fingerprint[1] = 0xFFFFFFFFFFFFFFFFULL;
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_GENETIC_SIM_FWD, 0, &sim));
    biosim_grid_set(&sim.grid, fwd, BIOSIM_GRID_EMPTY);
}

/* ── POPULATION_FWD ─────────────────────────────────────────────────────── */

void test_population_fwd_empty(void) {
    /* dir=0 (East), no agents → front=0, rear=0 → 0.5 */
    sim.agents.last_move_dir[0] = 0;
    biosim_grid_zero_fill(&sim.grid);
    TEST_ASSERT_EQUAL_FLOAT(0.5F, biosim_sensor_eval(BIOSIM_SENSOR_POPULATION_FWD, 0, &sim));
}

void test_population_fwd_all_occupied(void) {
    /* dir=0: all 4 front cells (x+1,y-1),(x+1,y),(x+1,y+1),(x+2,y) occupied →
     * front=4, rear=0 → (4-0)/(4+0)*0.5+0.5 = 1.0 */
    sim.agents.last_move_dir[0] = 0;
    biosim_grid_zero_fill(&sim.grid);
    int32_t x = sim.agents.loc_x[0];
    int32_t y = sim.agents.loc_y[0];
    biosim_grid_set(&sim.grid, (biosim_coord_t){x + 1, y - 1}, 2U);
    biosim_grid_set(&sim.grid, (biosim_coord_t){x + 1, y}, 2U);
    biosim_grid_set(&sim.grid, (biosim_coord_t){x + 1, y + 1}, 2U);
    biosim_grid_set(&sim.grid, (biosim_coord_t){x + 2, y}, 2U);
    TEST_ASSERT_EQUAL_FLOAT(1.0F, biosim_sensor_eval(BIOSIM_SENSOR_POPULATION_FWD, 0, &sim));
    biosim_grid_zero_fill(&sim.grid);
}

void test_population_fwd_backward_only(void) {
    /* dir=0: agent only in rear half → front=0, rear=1 → 0.0 */
    sim.agents.last_move_dir[0] = 0;
    biosim_grid_zero_fill(&sim.grid);
    int32_t x = sim.agents.loc_x[0];
    int32_t y = sim.agents.loc_y[0];
    biosim_grid_set(&sim.grid, (biosim_coord_t){x - 2, y}, 2U);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_POPULATION_FWD, 0, &sim));
    biosim_grid_set(&sim.grid, (biosim_coord_t){x - 2, y}, BIOSIM_GRID_EMPTY);
}

void test_population_fwd_symmetric(void) {
    /* dir=0: equal agents front and rear → front=2, rear=2 → 0.5 */
    sim.agents.last_move_dir[0] = 0;
    biosim_grid_zero_fill(&sim.grid);
    int32_t x = sim.agents.loc_x[0];
    int32_t y = sim.agents.loc_y[0];
    biosim_grid_set(&sim.grid, (biosim_coord_t){x + 1, y}, 2U);
    biosim_grid_set(&sim.grid, (biosim_coord_t){x + 2, y}, 2U);
    biosim_grid_set(&sim.grid, (biosim_coord_t){x - 1, y}, 2U);
    biosim_grid_set(&sim.grid, (biosim_coord_t){x - 2, y}, 2U);
    TEST_ASSERT_EQUAL_FLOAT(0.5F, biosim_sensor_eval(BIOSIM_SENSOR_POPULATION_FWD, 0, &sim));
    biosim_grid_zero_fill(&sim.grid);
}

/* ── LONGPROBE_POP_FWD ──────────────────────────────────────────────────── */

void test_longprobe_pop_fwd_empty(void) {
    /* dir=0 (East), clear path → 0.0 */
    sim.agents.last_move_dir[0] = 0;
    biosim_grid_zero_fill(&sim.grid);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_LONGPROBE_POP_FWD, 0, &sim));
}

void test_longprobe_pop_fwd_hit_step1(void) {
    /* agent at (9,8) — 1 step east → 1/16 */
    sim.agents.last_move_dir[0] = 0;
    biosim_grid_zero_fill(&sim.grid);
    int32_t x = sim.agents.loc_x[0];
    int32_t y = sim.agents.loc_y[0];
    biosim_grid_set(&sim.grid, (biosim_coord_t){x + 1, y}, 2U);
    TEST_ASSERT_FLOAT_WITHIN(
        1e-6F, 1.0F / 16.0F, biosim_sensor_eval(BIOSIM_SENSOR_LONGPROBE_POP_FWD, 0, &sim)
    );
    biosim_grid_set(&sim.grid, (biosim_coord_t){x + 1, y}, BIOSIM_GRID_EMPTY);
}

void test_longprobe_pop_fwd_hit_step2(void) {
    /* agent at (10,8) — 2 steps east → 2/16 */
    sim.agents.last_move_dir[0] = 0;
    biosim_grid_zero_fill(&sim.grid);
    int32_t x = sim.agents.loc_x[0];
    int32_t y = sim.agents.loc_y[0];
    biosim_grid_set(&sim.grid, (biosim_coord_t){x + 2, y}, 2U);
    TEST_ASSERT_FLOAT_WITHIN(
        1e-6F, 2.0F / 16.0F, biosim_sensor_eval(BIOSIM_SENSOR_LONGPROBE_POP_FWD, 0, &sim)
    );
    biosim_grid_set(&sim.grid, (biosim_coord_t){x + 2, y}, BIOSIM_GRID_EMPTY);
}

void test_longprobe_pop_fwd_barrier_stops(void) {
    /* barrier at step 2, agent at step 3 → barrier terminates probe → 0.0 */
    sim.agents.last_move_dir[0] = 0;
    biosim_grid_zero_fill(&sim.grid);
    int32_t x = sim.agents.loc_x[0];
    int32_t y = sim.agents.loc_y[0];
    biosim_grid_set(&sim.grid, (biosim_coord_t){x + 2, y}, BIOSIM_GRID_BARRIER);
    biosim_grid_set(&sim.grid, (biosim_coord_t){x + 3, y}, 2U);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_LONGPROBE_POP_FWD, 0, &sim));
    biosim_grid_set(&sim.grid, (biosim_coord_t){x + 2, y}, BIOSIM_GRID_EMPTY);
    biosim_grid_set(&sim.grid, (biosim_coord_t){x + 3, y}, BIOSIM_GRID_EMPTY);
}

void test_longprobe_pop_fwd_dist_zero(void) {
    /* dist=0 → immediate 0.0 regardless of grid */
    sim.agents.last_move_dir[0] = 0;
    sim.agents.los_range[0] = 0U;
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_LONGPROBE_POP_FWD, 0, &sim));
    sim.agents.los_range[0] = 16U;
}

/* ── POPULATION_LR ──────────────────────────────────────────────────────── */

void test_population_lr_empty(void) {
    /* dir=0 (East), no agents in disc → L=0, R=0 → 0.0 */
    sim.agents.last_move_dir[0] = 0;
    biosim_grid_zero_fill(&sim.grid);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_POPULATION_LR, 0, &sim));
}

void test_population_lr_left_only(void) {
    /* dir=0: lateral = -dy; left cells (dy<0): (7,7),(8,6),(8,7),(9,7) → L=4,R=0 → 1.0 */
    sim.agents.last_move_dir[0] = 0;
    biosim_grid_zero_fill(&sim.grid);
    int32_t x = sim.agents.loc_x[0];
    int32_t y = sim.agents.loc_y[0];
    biosim_grid_set(&sim.grid, (biosim_coord_t){x - 1, y - 1}, 2U);
    biosim_grid_set(&sim.grid, (biosim_coord_t){x, y - 2}, 2U);
    biosim_grid_set(&sim.grid, (biosim_coord_t){x, y - 1}, 2U);
    biosim_grid_set(&sim.grid, (biosim_coord_t){x + 1, y - 1}, 2U);
    TEST_ASSERT_EQUAL_FLOAT(1.0F, biosim_sensor_eval(BIOSIM_SENSOR_POPULATION_LR, 0, &sim));
    biosim_grid_zero_fill(&sim.grid);
}

void test_population_lr_right_only(void) {
    /* dir=0: right cells (dy>0): (7,9),(8,10),(8,9),(9,9) → L=0,R=4 → -1.0 */
    sim.agents.last_move_dir[0] = 0;
    biosim_grid_zero_fill(&sim.grid);
    int32_t x = sim.agents.loc_x[0];
    int32_t y = sim.agents.loc_y[0];
    biosim_grid_set(&sim.grid, (biosim_coord_t){x - 1, y + 1}, 2U);
    biosim_grid_set(&sim.grid, (biosim_coord_t){x, y + 2}, 2U);
    biosim_grid_set(&sim.grid, (biosim_coord_t){x, y + 1}, 2U);
    biosim_grid_set(&sim.grid, (biosim_coord_t){x + 1, y + 1}, 2U);
    TEST_ASSERT_EQUAL_FLOAT(-1.0F, biosim_sensor_eval(BIOSIM_SENSOR_POPULATION_LR, 0, &sim));
    biosim_grid_zero_fill(&sim.grid);
}

void test_population_lr_balanced(void) {
    /* 2 left, 2 right → (2-2)/(2+2) = 0.0 */
    sim.agents.last_move_dir[0] = 0;
    biosim_grid_zero_fill(&sim.grid);
    int32_t x = sim.agents.loc_x[0];
    int32_t y = sim.agents.loc_y[0];
    biosim_grid_set(&sim.grid, (biosim_coord_t){x - 1, y - 1}, 2U);
    biosim_grid_set(&sim.grid, (biosim_coord_t){x + 1, y - 1}, 2U);
    biosim_grid_set(&sim.grid, (biosim_coord_t){x - 1, y + 1}, 2U);
    biosim_grid_set(&sim.grid, (biosim_coord_t){x + 1, y + 1}, 2U);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_POPULATION_LR, 0, &sim));
    biosim_grid_zero_fill(&sim.grid);
}

void test_population_lr_asymmetric(void) {
    /* 2 left, 1 right → (2-1)/(2+1) = 1/3 */
    sim.agents.last_move_dir[0] = 0;
    biosim_grid_zero_fill(&sim.grid);
    int32_t x = sim.agents.loc_x[0];
    int32_t y = sim.agents.loc_y[0];
    biosim_grid_set(&sim.grid, (biosim_coord_t){x - 1, y - 1}, 2U);
    biosim_grid_set(&sim.grid, (biosim_coord_t){x, y - 1}, 2U);
    biosim_grid_set(&sim.grid, (biosim_coord_t){x - 1, y + 1}, 2U);
    TEST_ASSERT_FLOAT_WITHIN(
        1e-6F, 1.0F / 3.0F, biosim_sensor_eval(BIOSIM_SENSOR_POPULATION_LR, 0, &sim)
    );
    biosim_grid_zero_fill(&sim.grid);
}

void test_population_lr_axis_only(void) {
    /* axis cell (x+1,y) has lateral=0 → excluded; L=0,R=0 → 0.0 */
    sim.agents.last_move_dir[0] = 0;
    biosim_grid_zero_fill(&sim.grid);
    int32_t x = sim.agents.loc_x[0];
    int32_t y = sim.agents.loc_y[0];
    biosim_grid_set(&sim.grid, (biosim_coord_t){x + 1, y}, 2U);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_POPULATION_LR, 0, &sim));
    biosim_grid_set(&sim.grid, (biosim_coord_t){x + 1, y}, BIOSIM_GRID_EMPTY);
}

/* ── BARRIER_FWD ─────────────────────────────────────────────────────────── */

void test_barrier_fwd_empty(void) {
    /* dir=0 (East), no barriers in forward half-disc → 0.0 */
    sim.agents.last_move_dir[0] = 0;
    biosim_grid_zero_fill(&sim.grid);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_BARRIER_FWD, 0, &sim));
}

void test_barrier_fwd_one_forward(void) {
    /* barrier at (x+1,y); forward half-disc r=2 dir=0 has 4 cells → 1/4 */
    sim.agents.last_move_dir[0] = 0;
    biosim_grid_zero_fill(&sim.grid);
    int32_t x = sim.agents.loc_x[0];
    int32_t y = sim.agents.loc_y[0];
    biosim_grid_set(&sim.grid, (biosim_coord_t){x + 1, y}, BIOSIM_GRID_BARRIER);
    TEST_ASSERT_FLOAT_WITHIN(
        1e-6F, 1.0F / 4.0F, biosim_sensor_eval(BIOSIM_SENSOR_BARRIER_FWD, 0, &sim)
    );
    biosim_grid_set(&sim.grid, (biosim_coord_t){x + 1, y}, BIOSIM_GRID_EMPTY);
}

void test_barrier_fwd_all_forward(void) {
    /* all 4 forward cells are barriers → 1.0 */
    sim.agents.last_move_dir[0] = 0;
    biosim_grid_zero_fill(&sim.grid);
    int32_t x = sim.agents.loc_x[0];
    int32_t y = sim.agents.loc_y[0];
    biosim_grid_set(&sim.grid, (biosim_coord_t){x + 1, y}, BIOSIM_GRID_BARRIER);
    biosim_grid_set(&sim.grid, (biosim_coord_t){x + 1, y - 1}, BIOSIM_GRID_BARRIER);
    biosim_grid_set(&sim.grid, (biosim_coord_t){x + 1, y + 1}, BIOSIM_GRID_BARRIER);
    biosim_grid_set(&sim.grid, (biosim_coord_t){x + 2, y}, BIOSIM_GRID_BARRIER);
    TEST_ASSERT_EQUAL_FLOAT(1.0F, biosim_sensor_eval(BIOSIM_SENSOR_BARRIER_FWD, 0, &sim));
    biosim_grid_zero_fill(&sim.grid);
}

void test_barrier_fwd_backward_only(void) {
    /* barrier at (x-1,y) is behind → filtered out → 0.0 */
    sim.agents.last_move_dir[0] = 0;
    biosim_grid_zero_fill(&sim.grid);
    int32_t x = sim.agents.loc_x[0];
    int32_t y = sim.agents.loc_y[0];
    biosim_grid_set(&sim.grid, (biosim_coord_t){x - 1, y}, BIOSIM_GRID_BARRIER);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_BARRIER_FWD, 0, &sim));
    biosim_grid_set(&sim.grid, (biosim_coord_t){x - 1, y}, BIOSIM_GRID_EMPTY);
}

/* ── BARRIER_LR ──────────────────────────────────────────────────────────── */

void test_barrier_lr_empty(void) {
    /* dir=0 (East), no barriers → L=0, R=0 → 0.0 */
    sim.agents.last_move_dir[0] = 0;
    biosim_grid_zero_fill(&sim.grid);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_BARRIER_LR, 0, &sim));
}

void test_barrier_lr_left_only(void) {
    /* dir=0: lateral=-dy; left=dy<0 (North side): 4 cells → L=4,R=0 → 1.0 */
    sim.agents.last_move_dir[0] = 0;
    biosim_grid_zero_fill(&sim.grid);
    int32_t x = sim.agents.loc_x[0];
    int32_t y = sim.agents.loc_y[0];
    biosim_grid_set(&sim.grid, (biosim_coord_t){x, y - 1}, BIOSIM_GRID_BARRIER);
    biosim_grid_set(&sim.grid, (biosim_coord_t){x, y - 2}, BIOSIM_GRID_BARRIER);
    biosim_grid_set(&sim.grid, (biosim_coord_t){x - 1, y - 1}, BIOSIM_GRID_BARRIER);
    biosim_grid_set(&sim.grid, (biosim_coord_t){x + 1, y - 1}, BIOSIM_GRID_BARRIER);
    TEST_ASSERT_EQUAL_FLOAT(1.0F, biosim_sensor_eval(BIOSIM_SENSOR_BARRIER_LR, 0, &sim));
    biosim_grid_zero_fill(&sim.grid);
}

void test_barrier_lr_right_only(void) {
    /* dir=0: right=dy>0 (South side): 4 cells → L=0,R=4 → -1.0 */
    sim.agents.last_move_dir[0] = 0;
    biosim_grid_zero_fill(&sim.grid);
    int32_t x = sim.agents.loc_x[0];
    int32_t y = sim.agents.loc_y[0];
    biosim_grid_set(&sim.grid, (biosim_coord_t){x, y + 1}, BIOSIM_GRID_BARRIER);
    biosim_grid_set(&sim.grid, (biosim_coord_t){x, y + 2}, BIOSIM_GRID_BARRIER);
    biosim_grid_set(&sim.grid, (biosim_coord_t){x - 1, y + 1}, BIOSIM_GRID_BARRIER);
    biosim_grid_set(&sim.grid, (biosim_coord_t){x + 1, y + 1}, BIOSIM_GRID_BARRIER);
    TEST_ASSERT_EQUAL_FLOAT(-1.0F, biosim_sensor_eval(BIOSIM_SENSOR_BARRIER_LR, 0, &sim));
    biosim_grid_zero_fill(&sim.grid);
}

void test_barrier_lr_balanced(void) {
    /* 1 left + 1 right → (1-1)/(1+1) = 0.0 */
    sim.agents.last_move_dir[0] = 0;
    biosim_grid_zero_fill(&sim.grid);
    int32_t x = sim.agents.loc_x[0];
    int32_t y = sim.agents.loc_y[0];
    biosim_grid_set(&sim.grid, (biosim_coord_t){x, y - 1}, BIOSIM_GRID_BARRIER);
    biosim_grid_set(&sim.grid, (biosim_coord_t){x, y + 1}, BIOSIM_GRID_BARRIER);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_BARRIER_LR, 0, &sim));
    biosim_grid_zero_fill(&sim.grid);
}

/* ── LONGPROBE_BAR_FWD ───────────────────────────────────────────────────── */

void test_longprobe_bar_fwd_empty(void) {
    /* dir=0 (East), clear path → 0.0 */
    sim.agents.last_move_dir[0] = 0;
    biosim_grid_zero_fill(&sim.grid);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_LONGPROBE_BAR_FWD, 0, &sim));
}

void test_longprobe_bar_fwd_hit_step1(void) {
    /* barrier 1 step east → 1/16 */
    sim.agents.last_move_dir[0] = 0;
    biosim_grid_zero_fill(&sim.grid);
    int32_t x = sim.agents.loc_x[0];
    int32_t y = sim.agents.loc_y[0];
    biosim_grid_set(&sim.grid, (biosim_coord_t){x + 1, y}, BIOSIM_GRID_BARRIER);
    TEST_ASSERT_FLOAT_WITHIN(
        1e-6F, 1.0F / 16.0F, biosim_sensor_eval(BIOSIM_SENSOR_LONGPROBE_BAR_FWD, 0, &sim)
    );
    biosim_grid_set(&sim.grid, (biosim_coord_t){x + 1, y}, BIOSIM_GRID_EMPTY);
}

void test_longprobe_bar_fwd_hit_step2(void) {
    /* barrier 2 steps east → 2/16 */
    sim.agents.last_move_dir[0] = 0;
    biosim_grid_zero_fill(&sim.grid);
    int32_t x = sim.agents.loc_x[0];
    int32_t y = sim.agents.loc_y[0];
    biosim_grid_set(&sim.grid, (biosim_coord_t){x + 2, y}, BIOSIM_GRID_BARRIER);
    TEST_ASSERT_FLOAT_WITHIN(
        1e-6F, 2.0F / 16.0F, biosim_sensor_eval(BIOSIM_SENSOR_LONGPROBE_BAR_FWD, 0, &sim)
    );
    biosim_grid_set(&sim.grid, (biosim_coord_t){x + 2, y}, BIOSIM_GRID_EMPTY);
}

void test_longprobe_bar_fwd_agent_skipped(void) {
    /* agent at step 1, barrier at step 2 → probe skips agent → 2/16 */
    sim.agents.last_move_dir[0] = 0;
    biosim_grid_zero_fill(&sim.grid);
    int32_t x = sim.agents.loc_x[0];
    int32_t y = sim.agents.loc_y[0];
    biosim_grid_set(&sim.grid, (biosim_coord_t){x + 1, y}, 2U);
    biosim_grid_set(&sim.grid, (biosim_coord_t){x + 2, y}, BIOSIM_GRID_BARRIER);
    TEST_ASSERT_FLOAT_WITHIN(
        1e-6F, 2.0F / 16.0F, biosim_sensor_eval(BIOSIM_SENSOR_LONGPROBE_BAR_FWD, 0, &sim)
    );
    biosim_grid_set(&sim.grid, (biosim_coord_t){x + 1, y}, BIOSIM_GRID_EMPTY);
    biosim_grid_set(&sim.grid, (biosim_coord_t){x + 2, y}, BIOSIM_GRID_EMPTY);
}

void test_longprobe_bar_fwd_dist_zero(void) {
    /* dist=0 → immediate 0.0 */
    sim.agents.last_move_dir[0] = 0;
    sim.agents.los_range[0] = 0U;
    TEST_ASSERT_EQUAL_FLOAT(0.0F, biosim_sensor_eval(BIOSIM_SENSOR_LONGPROBE_BAR_FWD, 0, &sim));
    sim.agents.los_range[0] = 16U;
}

/* ── SET_RESPONSIVENESS ─────────────────────────────────────────────────── */

void test_set_responsiveness_zero_val(void) {
    /* tanh(0)*0.5 + 0.5 = 0.5 */
    biosim_action_apply(BIOSIM_ACTION_SET_RESPONSIVENESS, 0.0F, 0, &sim);
    TEST_ASSERT_FLOAT_WITHIN(1e-5F, 0.5F, sim.agents.responsiveness[0]);
}

void test_set_responsiveness_positive_val(void) {
    biosim_action_apply(BIOSIM_ACTION_SET_RESPONSIVENESS, 1.0F, 0, &sim);
    float expected = tanhf(1.0F) * 0.5F + 0.5F;
    TEST_ASSERT_FLOAT_WITHIN(1e-5F, expected, sim.agents.responsiveness[0]);
}

/* ── SET_OSCILLATOR_PERIOD ──────────────────────────────────────────────── */

void test_set_oscillator_period_large_positive(void) {
    biosim_action_apply(BIOSIM_ACTION_SET_OSCILLATOR_PERIOD, 100.0F, 0, &sim);
    TEST_ASSERT_TRUE(sim.agents.osc_period[0] <= 2048U);
    TEST_ASSERT_TRUE(sim.agents.osc_period[0] >= 2U);
}

void test_set_oscillator_period_large_negative(void) {
    biosim_action_apply(BIOSIM_ACTION_SET_OSCILLATOR_PERIOD, -100.0F, 0, &sim);
    TEST_ASSERT_TRUE(sim.agents.osc_period[0] >= 2U);
}

/* ── SET_LONGPROBE_DIST ─────────────────────────────────────────────────── */

void test_set_longprobe_dist_large_negative(void) {
    biosim_action_apply(BIOSIM_ACTION_SET_LONGPROBE_DIST, -100.0F, 0, &sim);
    TEST_ASSERT_EQUAL_UINT8(1U, sim.agents.los_range[0]);
}

void test_set_longprobe_dist_large_positive(void) {
    biosim_action_apply(BIOSIM_ACTION_SET_LONGPROBE_DIST, 100.0F, 0, &sim);
    TEST_ASSERT_EQUAL_UINT8(32U, sim.agents.los_range[0]);
}

/* ── movement accumulators ──────────────────────────────────────────────── */

void test_move_x_accumulates(void) {
    sim.agents.responsiveness[0] = 1.0F;
    sim.agents.dx_sum[0] = 0.0F;
    sim.agents.dy_sum[0] = 0.0F;
    biosim_action_apply(BIOSIM_ACTION_MOVE_X, 1.0F, 0, &sim);
    biosim_action_apply(BIOSIM_ACTION_MOVE_X, 0.5F, 0, &sim);
    TEST_ASSERT_FLOAT_WITHIN(1e-5F, 1.5F, sim.agents.dx_sum[0]);
}

void test_move_forward_east_positive_dx(void) {
    sim.agents.last_move_dir[0] = 0; /* E: DIR_DX=+1 */
    sim.agents.responsiveness[0] = 1.0F;
    sim.agents.dx_sum[0] = 0.0F;
    sim.agents.dy_sum[0] = 0.0F;
    biosim_action_apply(BIOSIM_ACTION_MOVE_FORWARD, 1.0F, 0, &sim);
    TEST_ASSERT_TRUE(sim.agents.dx_sum[0] > 0.0F);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, sim.agents.dy_sum[0]);
}

void test_move_reverse_of_east_is_west(void) {
    sim.agents.last_move_dir[0] = 0; /* E: reverse = W */
    sim.agents.responsiveness[0] = 1.0F;
    sim.agents.dx_sum[0] = 0.0F;
    sim.agents.dy_sum[0] = 0.0F;
    biosim_action_apply(BIOSIM_ACTION_MOVE_REVERSE, 1.0F, 0, &sim);
    TEST_ASSERT_TRUE(sim.agents.dx_sum[0] < 0.0F);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, sim.agents.dy_sum[0]);
}

void test_move_left_of_east_is_north(void) {
    sim.agents.last_move_dir[0] = 0; /* E: left = N → dy=-1 */
    sim.agents.responsiveness[0] = 1.0F;
    sim.agents.dx_sum[0] = 0.0F;
    sim.agents.dy_sum[0] = 0.0F;
    biosim_action_apply(BIOSIM_ACTION_MOVE_LEFT, 1.0F, 0, &sim);
    TEST_ASSERT_TRUE(sim.agents.dy_sum[0] < 0.0F);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, sim.agents.dx_sum[0]);
}

void test_move_right_of_east_is_south(void) {
    sim.agents.last_move_dir[0] = 0; /* E: right = S → dy=+1 */
    sim.agents.responsiveness[0] = 1.0F;
    sim.agents.dx_sum[0] = 0.0F;
    sim.agents.dy_sum[0] = 0.0F;
    biosim_action_apply(BIOSIM_ACTION_MOVE_RIGHT, 1.0F, 0, &sim);
    TEST_ASSERT_TRUE(sim.agents.dy_sum[0] > 0.0F);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, sim.agents.dx_sum[0]);
}

void test_move_forward_north_positive_dy(void) {
    sim.agents.last_move_dir[0] = 2; /* N: DIR_DY=-1 */
    sim.agents.responsiveness[0] = 1.0F;
    sim.agents.dx_sum[0] = 0.0F;
    sim.agents.dy_sum[0] = 0.0F;
    biosim_action_apply(BIOSIM_ACTION_MOVE_FORWARD, 1.0F, 0, &sim);
    TEST_ASSERT_TRUE(sim.agents.dy_sum[0] < 0.0F);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, sim.agents.dx_sum[0]);
}

void test_move_reverse_of_north_is_south(void) {
    sim.agents.last_move_dir[0] = 2; /* N: reverse = S */
    sim.agents.responsiveness[0] = 1.0F;
    sim.agents.dx_sum[0] = 0.0F;
    sim.agents.dy_sum[0] = 0.0F;
    biosim_action_apply(BIOSIM_ACTION_MOVE_REVERSE, 1.0F, 0, &sim);
    TEST_ASSERT_TRUE(sim.agents.dy_sum[0] > 0.0F);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, sim.agents.dx_sum[0]);
}

void test_move_left_of_north_is_west(void) {
    sim.agents.last_move_dir[0] = 2; /* N: left = W → dx=-1 */
    sim.agents.responsiveness[0] = 1.0F;
    sim.agents.dx_sum[0] = 0.0F;
    sim.agents.dy_sum[0] = 0.0F;
    biosim_action_apply(BIOSIM_ACTION_MOVE_LEFT, 1.0F, 0, &sim);
    TEST_ASSERT_TRUE(sim.agents.dx_sum[0] < 0.0F);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, sim.agents.dy_sum[0]);
}

void test_move_right_of_north_is_east(void) {
    sim.agents.last_move_dir[0] = 2; /* N: right = E → dx=+1 */
    sim.agents.responsiveness[0] = 1.0F;
    sim.agents.dx_sum[0] = 0.0F;
    sim.agents.dy_sum[0] = 0.0F;
    biosim_action_apply(BIOSIM_ACTION_MOVE_RIGHT, 1.0F, 0, &sim);
    TEST_ASSERT_TRUE(sim.agents.dx_sum[0] > 0.0F);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, sim.agents.dy_sum[0]);
}

/* ── cardinal movement accumulators ──────────────────────-──────────────── */

void test_move_east_cardinal(void) {
    sim.agents.responsiveness[0] = 1.0F;
    sim.agents.dx_sum[0] = 0.0F;
    sim.agents.dy_sum[0] = 0.0F;
    biosim_action_apply(BIOSIM_ACTION_MOVE_EAST, 0.0F, 0, &sim);
    TEST_ASSERT_TRUE(sim.agents.dx_sum[0] > 0.0F);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, sim.agents.dy_sum[0]);
}

void test_move_west_cardinal(void) {
    sim.agents.responsiveness[0] = 1.0F;
    sim.agents.dx_sum[0] = 0.0F;
    sim.agents.dy_sum[0] = 0.0F;
    biosim_action_apply(BIOSIM_ACTION_MOVE_WEST, 0.0F, 0, &sim);
    TEST_ASSERT_TRUE(sim.agents.dx_sum[0] < 0.0F);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, sim.agents.dy_sum[0]);
}

void test_move_north_cardinal(void) {
    sim.agents.responsiveness[0] = 1.0F;
    sim.agents.dx_sum[0] = 0.0F;
    sim.agents.dy_sum[0] = 0.0F;
    biosim_action_apply(BIOSIM_ACTION_MOVE_NORTH, 0.0F, 0, &sim);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, sim.agents.dx_sum[0]);
    TEST_ASSERT_TRUE(sim.agents.dy_sum[0] < 0.0F);
}

void test_move_south_cardinal(void) {
    sim.agents.responsiveness[0] = 1.0F;
    sim.agents.dx_sum[0] = 0.0F;
    sim.agents.dy_sum[0] = 0.0F;
    biosim_action_apply(BIOSIM_ACTION_MOVE_SOUTH, 0.0F, 0, &sim);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, sim.agents.dx_sum[0]);
    TEST_ASSERT_TRUE(sim.agents.dy_sum[0] > 0.0F);
}

/* ── EMIT_SIGNAL0 ───────────────────────────────────────────────────────── */

void test_emit_signal0_zero_val_no_emit(void) {
    memset(sim.signal, 0, sim.signal_len * sizeof(uint32_t));
    biosim_action_apply(BIOSIM_ACTION_EMIT_SIGNAL0, 0.0F, 0, &sim);
    size_t ci = (size_t)sim.agents.loc_y[0] * GRID_W + (size_t)sim.agents.loc_x[0];
    TEST_ASSERT_EQUAL_UINT32(0U, sim.signal[ci]);
}

void test_emit_signal0_negative_val_no_emit(void) {
    memset(sim.signal, 0, sim.signal_len * sizeof(uint32_t));
    sim.agents.responsiveness[0] = 1.0F;
    biosim_action_apply(BIOSIM_ACTION_EMIT_SIGNAL0, -1.0F, 0, &sim);
    size_t ci = (size_t)sim.agents.loc_y[0] * GRID_W + (size_t)sim.agents.loc_x[0];
    TEST_ASSERT_EQUAL_UINT32(0U, sim.signal[ci]);
}

void test_emit_signal0_center_max_act(void) {
    /* val=10 → tanhf(10)≈1, resp=1 → act≈1: r=5, center_mag=5 */
    memset(sim.signal, 0, sim.signal_len * sizeof(uint32_t));
    sim.agents.responsiveness[0] = 1.0F;
    biosim_action_apply(BIOSIM_ACTION_EMIT_SIGNAL0, 10.0F, 0, &sim);
    size_t ci = (size_t)sim.agents.loc_y[0] * GRID_W + (size_t)sim.agents.loc_x[0];
    TEST_ASSERT_EQUAL_UINT32(5U, sim.signal[ci]);
}

void test_emit_signal0_linear_decay(void) {
    /* act≈1: cardinal (floor_dist=1) gets 5-1=4; diagonal (floor_dist=1) gets 4 */
    memset(sim.signal, 0, sim.signal_len * sizeof(uint32_t));
    sim.agents.responsiveness[0] = 1.0F;
    biosim_action_apply(BIOSIM_ACTION_EMIT_SIGNAL0, 10.0F, 0, &sim);
    int32_t x = sim.agents.loc_x[0];
    int32_t y = sim.agents.loc_y[0];
    size_t cardinal = (size_t)y * GRID_W + (size_t)(x + 1);
    size_t diagonal = (size_t)(y + 1) * GRID_W + (size_t)(x + 1);
    TEST_ASSERT_EQUAL_UINT32(4U, sim.signal[cardinal]);
    TEST_ASSERT_EQUAL_UINT32(4U, sim.signal[diagonal]);
}

void test_emit_signal0_radius_clips(void) {
    /* act≈1: r=5, so cell at dx=6 is outside the disc → 0 */
    memset(sim.signal, 0, sim.signal_len * sizeof(uint32_t));
    sim.agents.responsiveness[0] = 1.0F;
    biosim_action_apply(BIOSIM_ACTION_EMIT_SIGNAL0, 10.0F, 0, &sim);
    int32_t x = sim.agents.loc_x[0];
    int32_t y = sim.agents.loc_y[0];
    size_t outside = (size_t)y * GRID_W + (size_t)(x + 6);
    TEST_ASSERT_EQUAL_UINT32(0U, sim.signal[outside]);
}

void test_emit_signal0_min_act_radius_one(void) {
    /* val=0.01 → act≈0.01: r=1, center_mag=2; cardinal gets 1, diagonal excluded */
    memset(sim.signal, 0, sim.signal_len * sizeof(uint32_t));
    sim.agents.responsiveness[0] = 1.0F;
    biosim_action_apply(BIOSIM_ACTION_EMIT_SIGNAL0, 0.01F, 0, &sim);
    int32_t x = sim.agents.loc_x[0];
    int32_t y = sim.agents.loc_y[0];
    size_t ci = (size_t)y * GRID_W + (size_t)x;
    size_t cardinal = (size_t)y * GRID_W + (size_t)(x + 1);
    size_t diagonal = (size_t)(y + 1) * GRID_W + (size_t)(x + 1);
    TEST_ASSERT_EQUAL_UINT32(2U, sim.signal[ci]);
    TEST_ASSERT_EQUAL_UINT32(1U, sim.signal[cardinal]);
    TEST_ASSERT_EQUAL_UINT32(0U, sim.signal[diagonal]);
}

void test_emit_signal0_clamped_at_255(void) {
    /* act≈1: center_mag=5; pre-fill with 251 → 251+5=256 clamps to 255 */
    memset(sim.signal, 0, sim.signal_len * sizeof(uint32_t));
    sim.agents.responsiveness[0] = 1.0F;
    size_t ci = (size_t)sim.agents.loc_y[0] * GRID_W + (size_t)sim.agents.loc_x[0];
    sim.signal[ci] = 251U;
    biosim_action_apply(BIOSIM_ACTION_EMIT_SIGNAL0, 10.0F, 0, &sim);
    TEST_ASSERT_EQUAL_UINT32(255U, sim.signal[ci]);
}

/* ── KILL_FORWARD ───────────────────────────────────────────────────────── */

void test_kill_forward_below_threshold(void) {
    sim.agents.last_move_dir[0] = 0;
    biosim_coord_t fwd = {sim.agents.loc_x[0] + 1, sim.agents.loc_y[0]};
    biosim_grid_set(&sim.grid, fwd, 2U); /* agent 1 */
    biosim_agents_init_slot(&sim.agents, 1, fwd, 16, 99ULL);

    biosim_action_apply(BIOSIM_ACTION_KILL_FORWARD, 0.0F, 0, &sim);
    TEST_ASSERT_EQUAL_UINT8(1U, sim.agents.alive[1]);

    biosim_grid_set(&sim.grid, fwd, BIOSIM_GRID_EMPTY);
}

void test_kill_forward_above_threshold(void) {
    sim.enable_kill = true;
    sim.agents.last_move_dir[0] = 0;
    biosim_coord_t fwd = {sim.agents.loc_x[0] + 1, sim.agents.loc_y[0]};
    biosim_grid_set(&sim.grid, fwd, 2U); /* agent 1 */
    biosim_agents_init_slot(&sim.agents, 1, fwd, 16, 99ULL);

    biosim_action_apply(BIOSIM_ACTION_KILL_FORWARD, 1.0F, 0, &sim);
    /* Kill is deferred: marker set, agent still alive, grid cell still occupied. */
    TEST_ASSERT_EQUAL_UINT8(1U, sim.agents.kill_marker[1]);
    TEST_ASSERT_EQUAL_UINT8(1U, sim.agents.alive[1]);
    TEST_ASSERT_EQUAL_UINT32(2U, biosim_grid_at(&sim.grid, fwd));

    sim.agents.kill_marker[1] = 0U;
    biosim_grid_set(&sim.grid, fwd, BIOSIM_GRID_EMPTY);
}

void test_kill_forward_empty_cell_no_crash(void) {
    sim.enable_kill = true;
    sim.agents.last_move_dir[0] = 0;
    biosim_coord_t fwd = {sim.agents.loc_x[0] + 1, sim.agents.loc_y[0]};
    biosim_grid_set(&sim.grid, fwd, BIOSIM_GRID_EMPTY);
    biosim_action_apply(BIOSIM_ACTION_KILL_FORWARD, 1.0F, 0, &sim);
    /* No assertion needed — just verifying no crash. */
}

/* ── finalize movement ──────────────────────────────────────────────────── */

void test_finalize_no_accumulation(void) {
    sim.agents.dx_sum[0] = 0.0F;
    sim.agents.dy_sum[0] = 0.0F;
    biosim_action_propose_move(0, &sim);
    /* With zero sums tanh gives 0, so the step probability is 0; desired == loc. */
    TEST_ASSERT_EQUAL_INT32(sim.agents.loc_x[0], sim.agents.desired_x[0]);
    TEST_ASSERT_EQUAL_INT32(sim.agents.loc_y[0], sim.agents.desired_y[0]);
}

void test_finalize_boundary_clamp_east(void) {
    sim.agents.loc_x[0] = GRID_W - 1;
    sim.agents.dx_sum[0] = 100.0F;
    sim.agents.dy_sum[0] = 0.0F;
    biosim_action_propose_move(0, &sim);
    TEST_ASSERT_TRUE(sim.agents.desired_x[0] < GRID_W);
}

void test_finalize_boundary_clamp_west(void) {
    sim.agents.loc_x[0] = 0;
    sim.agents.dx_sum[0] = -100.0F;
    sim.agents.dy_sum[0] = 0.0F;
    biosim_action_propose_move(0, &sim);
    TEST_ASSERT_TRUE(sim.agents.desired_x[0] >= 0);
}

void test_finalize_boundary_clamp_north(void) {
    sim.agents.loc_y[0] = 0;
    sim.agents.dx_sum[0] = 0.0F;
    sim.agents.dy_sum[0] = -100.0F;
    biosim_action_propose_move(0, &sim);
    TEST_ASSERT_TRUE(sim.agents.desired_y[0] >= 0);
}

void test_finalize_boundary_clamp_south(void) {
    sim.agents.loc_y[0] = GRID_H - 1;
    sim.agents.dx_sum[0] = 0.0F;
    sim.agents.dy_sum[0] = 100.0F;
    biosim_action_propose_move(0, &sim);
    TEST_ASSERT_TRUE(sim.agents.desired_y[0] < GRID_H);
}

void test_finalize_strong_east_steps_one_cell(void) {
    /* Large dx_sum: tanh(100*0.5)≈1.0 > any rng_float draw → step is certain. */
    sim.agents.loc_x[0] = GRID_W / 2;
    sim.agents.loc_y[0] = GRID_H / 2;
    int32_t start_x = sim.agents.loc_x[0];
    int32_t start_y = sim.agents.loc_y[0];
    sim.agents.dx_sum[0] = 100.0F;
    sim.agents.dy_sum[0] = 0.0F;
    biosim_action_propose_move(0, &sim);
    TEST_ASSERT_EQUAL_INT32(start_x + 1, sim.agents.desired_x[0]);
    TEST_ASSERT_EQUAL_INT32(start_y, sim.agents.desired_y[0]);
}

void test_finalize_strong_north_steps_one_cell(void) {
    /* Large negative dy_sum → certain northward step (decreasing y). */
    sim.agents.loc_x[0] = GRID_W / 2;
    sim.agents.loc_y[0] = GRID_H / 2;
    int32_t start_x = sim.agents.loc_x[0];
    int32_t start_y = sim.agents.loc_y[0];
    sim.agents.dx_sum[0] = 0.0F;
    sim.agents.dy_sum[0] = -100.0F;
    biosim_action_propose_move(0, &sim);
    TEST_ASSERT_EQUAL_INT32(start_x, sim.agents.desired_x[0]);
    TEST_ASSERT_EQUAL_INT32(start_y - 1, sim.agents.desired_y[0]);
}

/* ── runner ─────────────────────────────────────────────────────────────── */

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
    /* SIGNAL0_FWD */
    RUN_TEST(test_signal0_fwd_empty);
    RUN_TEST(test_signal0_fwd_all_max);
    RUN_TEST(test_signal0_fwd_backward_only);
    RUN_TEST(test_signal0_fwd_symmetric);
    RUN_TEST(test_signal0_fwd_over_255);
    /* SIGNAL0_LR */
    RUN_TEST(test_signal0_lr_empty);
    RUN_TEST(test_signal0_lr_left_only);
    RUN_TEST(test_signal0_lr_right_only);
    RUN_TEST(test_signal0_lr_balanced);
    RUN_TEST(test_signal0_lr_asymmetric);
    /* GENETIC_SIM_FWD */

    /* Remove implementation pending redesign of the
        genetic similarity sensor and its implementation on the GPU */
    // RUN_TEST(test_genetic_sim_fwd_empty_forward);
    // RUN_TEST(test_genetic_sim_fwd_barrier_forward);
    // RUN_TEST(test_genetic_sim_fwd_identical_fingerprint);
    // RUN_TEST(test_genetic_sim_fwd_all_bits_differ);

    /* POPULATION_FWD */
    RUN_TEST(test_population_fwd_empty);
    RUN_TEST(test_population_fwd_all_occupied);
    RUN_TEST(test_population_fwd_backward_only);
    RUN_TEST(test_population_fwd_symmetric);
    /* LONGPROBE_POP_FWD */
    RUN_TEST(test_longprobe_pop_fwd_empty);
    RUN_TEST(test_longprobe_pop_fwd_hit_step1);
    RUN_TEST(test_longprobe_pop_fwd_hit_step2);
    RUN_TEST(test_longprobe_pop_fwd_barrier_stops);
    RUN_TEST(test_longprobe_pop_fwd_dist_zero);
    /* POPULATION_LR */
    RUN_TEST(test_population_lr_empty);
    RUN_TEST(test_population_lr_left_only);
    RUN_TEST(test_population_lr_right_only);
    RUN_TEST(test_population_lr_balanced);
    RUN_TEST(test_population_lr_asymmetric);
    RUN_TEST(test_population_lr_axis_only);
    /* BARRIER_FWD */
    RUN_TEST(test_barrier_fwd_empty);
    RUN_TEST(test_barrier_fwd_one_forward);
    RUN_TEST(test_barrier_fwd_all_forward);
    RUN_TEST(test_barrier_fwd_backward_only);
    /* BARRIER_LR */
    RUN_TEST(test_barrier_lr_empty);
    RUN_TEST(test_barrier_lr_left_only);
    RUN_TEST(test_barrier_lr_right_only);
    RUN_TEST(test_barrier_lr_balanced);
    /* LONGPROBE_BAR_FWD */
    RUN_TEST(test_longprobe_bar_fwd_empty);
    RUN_TEST(test_longprobe_bar_fwd_hit_step1);
    RUN_TEST(test_longprobe_bar_fwd_hit_step2);
    RUN_TEST(test_longprobe_bar_fwd_agent_skipped);
    RUN_TEST(test_longprobe_bar_fwd_dist_zero);
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
    RUN_TEST(test_emit_signal0_zero_val_no_emit);
    RUN_TEST(test_emit_signal0_negative_val_no_emit);
    RUN_TEST(test_emit_signal0_center_max_act);
    RUN_TEST(test_emit_signal0_linear_decay);
    RUN_TEST(test_emit_signal0_radius_clips);
    RUN_TEST(test_emit_signal0_min_act_radius_one);
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
