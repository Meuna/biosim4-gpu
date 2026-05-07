#include "sim_test_utils.h"

#include "biosim/core/rng.h"
#include "biosim/core/sim.h"
#include "unity.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* ── sim factories ──────────────────────────────────────────────────────────── */

biosim_sim_t sim_test_make_light(void) {
    static uint32_t call_count = 0U;

    biosim_sim_t s;
    memset(&s, 0, sizeof(s));
    s.population = 4U;
    s.size_x = 4;
    s.size_y = 4;
    s.genome_max_len = 4U;
    s.max_neurons = 2U;
    s.long_probe_dist = 4U;
    s.steps_per_gen = 1;
    s.population_sensor_radius = 1;
    s.challenge.kind = BIOSIM_CHALLENGE_X_BAND;
    s.challenge.x_band.x_min = 0.0F;
    s.challenge.x_band.x_max = 1.0F;
    s.challenge.x_band.mirror = false;
    s.mutation_rate = 0.0F;
    s.gen_rng = biosim_rng_seed(0U, ++call_count);
    return s;
}

biosim_sim_t sim_test_make_medium(void) {
    static uint32_t call_count = 0U;

    biosim_sim_t s;
    memset(&s, 0, sizeof(s));
    s.population = 64U;
    s.size_x = 32;
    s.size_y = 32;
    s.genome_max_len = 24U;
    s.max_neurons = 3U;
    s.long_probe_dist = 8U;
    s.steps_per_gen = 16U;
    s.population_sensor_radius = 4;
    s.challenge.kind = BIOSIM_CHALLENGE_X_BAND;
    s.challenge.x_band.x_min = 0.25F;
    s.challenge.x_band.x_max = 0.75F;
    s.challenge.x_band.mirror = true;
    s.mutation_rate = 0.01F;
    s.choose_parents_by_fitness = true;
    s.sexual_reproduction = true;
    s.gen_rng = biosim_rng_seed(0U, ++call_count);
    return s;
}

void sim_test_run_one_gen(biosim_sim_t *sim) {
    while (sim->step < sim->steps_per_gen) {
        for (uint32_t i = 0U; i < sim->genome.population; i++) {
            biosim_sim_step_agent(sim, i);
        }
        biosim_sim_next_step(sim);
    }
}

/* ── equality assertions ────────────────────────────────────────────────────── */

void assert_agents_equal(const biosim_agents_t *a, const biosim_agents_t *b) {
    const uint32_t pop = a->population;
    TEST_ASSERT_EQUAL_UINT32(pop, b->population);
    for (uint32_t i = 0U; i < pop; i++) {
        TEST_ASSERT_EQUAL_INT16(a->loc_x[i], b->loc_x[i]);
        TEST_ASSERT_EQUAL_INT16(a->loc_y[i], b->loc_y[i]);
        TEST_ASSERT_EQUAL_INT16(a->birth_x[i], b->birth_x[i]);
        TEST_ASSERT_EQUAL_INT16(a->birth_y[i], b->birth_y[i]);
        TEST_ASSERT_EQUAL_UINT8(a->alive[i], b->alive[i]);
        TEST_ASSERT_EQUAL_UINT16(a->osc_period[i], b->osc_period[i]);
        TEST_ASSERT_EQUAL_FLOAT(a->responsiveness[i], b->responsiveness[i]);
        TEST_ASSERT_EQUAL_UINT8(a->long_probe_dist[i], b->long_probe_dist[i]);
        TEST_ASSERT_EQUAL_UINT8(a->last_move_dir[i], b->last_move_dir[i]);
        TEST_ASSERT_EQUAL_UINT32(a->challenge_bits[i], b->challenge_bits[i]);
        TEST_ASSERT_EQUAL_UINT64(a->rng_state[i], b->rng_state[i]);
        TEST_ASSERT_EQUAL_UINT64(a->genome_fingerprint[i], b->genome_fingerprint[i]);
        TEST_ASSERT_EQUAL_INT16(a->desired_x[i], b->desired_x[i]);
        TEST_ASSERT_EQUAL_INT16(a->desired_y[i], b->desired_y[i]);
        TEST_ASSERT_EQUAL_FLOAT(a->dx_sum[i], b->dx_sum[i]);
        TEST_ASSERT_EQUAL_FLOAT(a->dy_sum[i], b->dy_sum[i]);
    }
}

void assert_grid_equal(const biosim_grid_t *a, const biosim_grid_t *b) {
    TEST_ASSERT_EQUAL_INT16(a->size_x, b->size_x);
    TEST_ASSERT_EQUAL_INT16(a->size_y, b->size_y);
    const size_t n = (size_t)a->size_x * (size_t)a->size_y;
    for (size_t i = 0U; i < n; i++) {
        TEST_ASSERT_EQUAL_UINT16(a->cells[i], b->cells[i]);
    }
}

void assert_genome_slice_equal(const biosim_genome_t *a, const biosim_genome_t *b,
                               uint32_t n_agents) {
    TEST_ASSERT_EQUAL_UINT16(a->max_len, b->max_len);
    const uint32_t pop_a = a->population;
    const uint32_t pop_b = b->population;
    for (uint32_t s = 0U; s < n_agents; s++) {
        TEST_ASSERT_EQUAL_UINT16(a->len[s], b->len[s]);
        const uint16_t len = a->len[s];
        for (uint16_t j = 0U; j < len; j++) {
            TEST_ASSERT_EQUAL_UINT16(a->conn[(size_t)j * pop_a + s],
                                     b->conn[(size_t)j * pop_b + s]);
            TEST_ASSERT_EQUAL_INT16(a->wgt[(size_t)j * pop_a + s], b->wgt[(size_t)j * pop_b + s]);
        }
    }
}

void assert_genome_equal(const biosim_genome_t *a, const biosim_genome_t *b) {
    TEST_ASSERT_EQUAL_UINT32(a->population, b->population);
    assert_genome_slice_equal(a, b, a->population);
}

void assert_nnet_equal(const biosim_nnet_t *a, const biosim_nnet_t *b) {
    TEST_ASSERT_EQUAL_UINT32(a->population, b->population);
    TEST_ASSERT_EQUAL_UINT16(a->max_conn, b->max_conn);
    TEST_ASSERT_EQUAL_UINT8(a->max_neurons, b->max_neurons);
    const uint32_t pop = a->population;
    for (uint32_t s = 0U; s < pop; s++) {
        TEST_ASSERT_EQUAL_UINT16(a->conn_length[s], b->conn_length[s]);
        TEST_ASSERT_EQUAL_UINT8(a->neuron_count[s], b->neuron_count[s]);
        uint16_t conn_length = a->conn_length[s];
        for (uint16_t c = 0U; c < conn_length; c++) {
            TEST_ASSERT_EQUAL_UINT16(a->genome_conn[(size_t)c * pop + s],
                                     b->genome_conn[(size_t)c * pop + s]);
            TEST_ASSERT_EQUAL_INT16(a->genome_wgt[(size_t)c * pop + s],
                                    b->genome_wgt[(size_t)c * pop + s]);
        }
        uint8_t neuron_count = a->neuron_count[s];
        for (uint8_t n = 0U; n < neuron_count; n++) {
            TEST_ASSERT_EQUAL_UINT8(a->neuron_driven[(size_t)n * pop + s],
                                    b->neuron_driven[(size_t)n * pop + s]);
            TEST_ASSERT_EQUAL_FLOAT(a->neuron_output[(size_t)n * pop + s],
                                    b->neuron_output[(size_t)n * pop + s]);
        }
    }
}

void assert_sim_equal(const biosim_sim_t *a, const biosim_sim_t *b) {
    TEST_ASSERT_EQUAL_UINT32(a->gen, b->gen);
    TEST_ASSERT_EQUAL_UINT64(a->gen_rng, b->gen_rng);
    TEST_ASSERT_EQUAL_UINT32(a->step, b->step);
    TEST_ASSERT_EQUAL_UINT32(a->kills, b->kills);
    assert_agents_equal(&a->agents, &b->agents);
    assert_grid_equal(&a->grid, &b->grid);
    assert_genome_equal(&a->genome, &b->genome);
    assert_nnet_equal(&a->nnet, &b->nnet);
}
