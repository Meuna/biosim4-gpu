#include "biosim/core/test_utils.h"

#include "biosim/core/challenge_spec.h"
#include "biosim/core/params.h"
#include "biosim/core/sim.h"
#include "unity.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* ── sim factories ──────────────────────────────────────────────────────────── */

/* clang-format off */
static const biosim_param_entry_t k_test_params[] = {
    {"max-generations",           NULL,  {.i = 100},   PARAM_INT,   false, true, NULL, NULL},
    {"population",                NULL,  {.i = 4},     PARAM_INT,   false, true, NULL, NULL},
    {"grid-size-x",               NULL,  {.i = 4},     PARAM_INT,   false, true, NULL, NULL},
    {"grid-size-y",               NULL,  {.i = 4},     PARAM_INT,   false, true, NULL, NULL},
    {"max-genome-len",            NULL,  {.i = 4},     PARAM_INT,   false, true, NULL, NULL},
    {"max-neurons",               NULL,  {.i = 2},     PARAM_INT,   false, true, NULL, NULL},
    {"los-range",                 NULL,  {.i = 4},     PARAM_INT,   false, true, NULL, NULL},
    {"steps-per-gen",             NULL,  {.i = 1},     PARAM_INT,   false, true, NULL, NULL},
    {"sensor-radius",             NULL,  {.i = 1},     PARAM_INT,   false, true, NULL, NULL},
    {"enable-kill",               NULL,  {.b = false}, PARAM_BOOL,  false, true, NULL, NULL},
    {"responsiveness-curve-k",    NULL,  {.f = 2.0},   PARAM_FLOAT, false, true, NULL, NULL},
    {"point-mutation-rate",       NULL,  {.f = 0.0},   PARAM_FLOAT, false, true, NULL, NULL},
    {"sexual-reproduction",       NULL,  {.b = false}, PARAM_BOOL,  false, true, NULL, NULL},
    {"choose-parents-by-fitness", NULL,  {.b = false}, PARAM_BOOL,  false, true, NULL, NULL},
};
/* clang-format on */
#define K_TEST_PARAMS_COUNT (sizeof(k_test_params) / sizeof(k_test_params[0]))

biosim_status_t sim_test_create(biosim_sim_t *sim, const sim_test_scn_t *cfg) {
    biosim_params_t p;
    biosim_status_t rc = biosim_params_init(&p, k_test_params, K_TEST_PARAMS_COUNT);
    if (rc != BIOSIM_OK) {
        return rc;
    }

    (void)biosim_params_set_int(&p, "population", (int)cfg->population);
    (void)biosim_params_set_int(&p, "grid-size-x", (int)cfg->size_x);
    (void)biosim_params_set_int(&p, "grid-size-y", (int)cfg->size_y);
    (void)biosim_params_set_int(&p, "max-genome-len", (int)cfg->genome_max_len);
    (void)biosim_params_set_int(&p, "max-neurons", (int)cfg->max_neurons);
    (void)biosim_params_set_int(&p, "los-range", (int)cfg->los_range);
    (void)biosim_params_set_int(&p, "steps-per-gen", (int)cfg->steps_per_gen);
    (void)biosim_params_set_int(&p, "sensor-radius", (int)cfg->sensor_radius);
    (void)biosim_params_set_float(&p, "point-mutation-rate", (double)cfg->mutation_rate);
    (void)biosim_params_set_bool(&p, "sexual-reproduction", cfg->sexual_reproduction);
    (void)biosim_params_set_bool(&p, "choose-parents-by-fitness", cfg->choose_parents_by_fitness);

    biosim_challenge_spec_t challenge;
    memset(&challenge, 0, sizeof(challenge));
    challenge.kind = BIOSIM_CHALLENGE_X_BAND;
    challenge.x_band.x_min = 0.0F;
    challenge.x_band.x_max = 1.0F;
    challenge.x_band.mirror = false;

    rc = biosim_sim_create(sim, &p, &challenge, cfg->barrier_specs, cfg->n_barrier_specs);
    biosim_params_free(&p);
    return rc;
}

biosim_status_t sim_test_make_8x8(biosim_sim_t *sim) {
    return sim_test_create(
        sim,
        &(sim_test_scn_t){
            .population = 4U,
            .size_x = 8,
            .size_y = 8,
            .genome_max_len = 4U,
            .max_neurons = 2U,
            .los_range = 4U,
            .steps_per_gen = 1U,
            .sensor_radius = 1,
        }
    );
}

biosim_status_t sim_test_make_32x32(biosim_sim_t *sim) {
    static const biosim_barrier_spec_t k_barriers[] = {
        {BIOSIM_BARRIER_HBAR, 16, 16, 20.0F, 1.0F},
    };
    return sim_test_create(
        sim,
        &(sim_test_scn_t){
            .population = 64U,
            .size_x = 32,
            .size_y = 32,
            .genome_max_len = 24U,
            .max_neurons = 3U,
            .los_range = 8U,
            .steps_per_gen = 16U,
            .sensor_radius = 4,
            .mutation_rate = 0.01F,
            .sexual_reproduction = true,
            .choose_parents_by_fitness = true,
            .barrier_specs = k_barriers,
            .n_barrier_specs = 1U,
        }
    );
}

biosim_status_t sim_test_make_4x4_pop6(biosim_sim_t *sim) {
    return sim_test_create(
        sim,
        &(sim_test_scn_t){
            .population = 6U,
            .size_x = 4,
            .size_y = 4,
            .genome_max_len = 4U,
            .max_neurons = 2U,
            .los_range = 4U,
            .steps_per_gen = 1U,
            .sensor_radius = 1,
        }
    );
}

biosim_status_t sim_test_make_128x128(biosim_sim_t *sim) {
    static const biosim_barrier_spec_t k_barriers[] = {
        {BIOSIM_BARRIER_HBAR, 21, 32, 32.0F, 4.0F},
        {BIOSIM_BARRIER_HBAR, 21, 96, 32.0F, 4.0F},
        {BIOSIM_BARRIER_HBAR, 42, 64, 64.0F, 4.0F},
        {BIOSIM_BARRIER_HBAR, 64, 32, 32.0F, 4.0F},
        {BIOSIM_BARRIER_HBAR, 64, 96, 32.0F, 4.0F},
        {BIOSIM_BARRIER_HBAR, 85, 64, 64.0F, 4.0F},
        {BIOSIM_BARRIER_HBAR, 106, 32, 32.0F, 4.0F},
        {BIOSIM_BARRIER_HBAR, 106, 96, 32.0F, 4.0F},
    };
    return sim_test_create(
        sim,
        &(sim_test_scn_t){
            .population = 3000U,
            .size_x = 128,
            .size_y = 128,
            .genome_max_len = 24U,
            .max_neurons = 5U,
            .los_range = 8U,
            .steps_per_gen = 16U,
            .sensor_radius = 4,
            .mutation_rate = 0.01F,
            .sexual_reproduction = true,
            .choose_parents_by_fitness = true,
            .barrier_specs = k_barriers,
            .n_barrier_specs = 8U,
        }
    );
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
        TEST_ASSERT_EQUAL_UINT8(a->los_range[i], b->los_range[i]);
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

void assert_genome_slice_equal(
    const biosim_genome_t *a, const biosim_genome_t *b, uint32_t n_agents
) {
    TEST_ASSERT_EQUAL_UINT16(a->max_len, b->max_len);
    const uint32_t pop_a = a->population;
    const uint32_t pop_b = b->population;
    for (uint32_t s = 0U; s < n_agents; s++) {
        TEST_ASSERT_EQUAL_UINT16(a->len[s], b->len[s]);
        const uint16_t len = a->len[s];
        for (uint16_t j = 0U; j < len; j++) {
            TEST_ASSERT_EQUAL_UINT16(
                a->conn[(size_t)j * pop_a + s], b->conn[(size_t)j * pop_b + s]
            );
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
            TEST_ASSERT_EQUAL_UINT16(
                a->genome_conn[(size_t)c * pop + s], b->genome_conn[(size_t)c * pop + s]
            );
            TEST_ASSERT_EQUAL_INT16(
                a->genome_wgt[(size_t)c * pop + s], b->genome_wgt[(size_t)c * pop + s]
            );
        }
        uint8_t neuron_count = a->neuron_count[s];
        for (uint8_t n = 0U; n < neuron_count; n++) {
            TEST_ASSERT_EQUAL_UINT8(
                a->neuron_driven[(size_t)n * pop + s], b->neuron_driven[(size_t)n * pop + s]
            );
            TEST_ASSERT_EQUAL_FLOAT(
                a->neuron_output[(size_t)n * pop + s], b->neuron_output[(size_t)n * pop + s]
            );
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
