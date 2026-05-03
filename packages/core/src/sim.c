#include "biosim/core/sim.h"

#include "biosim/core/census.h"
#include "biosim/core/challenges.h"
#include "biosim/core/generation.h"
#include "biosim/core/io_catalogue.h"
#include "biosim/core/rng.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* ── lifecycle ──────────────────────────────────────────────────────────── */

biosim_status_t biosim_sim_create(biosim_sim_t *sim, const biosim_barrier_spec_t *barriers,
                                  int n_barriers) {
    biosim_status_t st;

    const uint32_t pop = sim->population;
    const int16_t size_x = sim->size_x;
    const int16_t size_y = sim->size_y;
    const uint16_t max_gen_len = sim->max_gen_len;
    const uint8_t max_neurons = sim->max_neurons;

    sim->kills = 0;

    st = biosim_grid_create(size_x, size_y, &sim->grid);
    if (st != BIOSIM_OK) {
        biosim_sim_free(sim);
        return st;
    }

    sim->barrier_ctrs = NULL;
    sim->n_barrier_ctrs = 0;
    if (n_barriers > 0) {
        sim->barrier_ctrs = (biosim_coord_t *)malloc((size_t)n_barriers * sizeof(biosim_coord_t));
        if (sim->barrier_ctrs == NULL) {
            biosim_sim_free(sim);
            return BIOSIM_ERR_NOMEM;
        }
        uint64_t barrier_rng = biosim_rng_seed(0, 0);
        st = biosim_barriers_place(&sim->grid, barriers, n_barriers, &barrier_rng,
                                   sim->barrier_ctrs);
        if (st != BIOSIM_OK) {
            biosim_sim_free(sim);
            return st;
        }
        sim->n_barrier_ctrs = n_barriers;
    }

    st = biosim_agents_create(pop, &sim->agents);
    if (st != BIOSIM_OK) {
        biosim_sim_free(sim);
        return st;
    }

    st = biosim_genome_create(pop, max_gen_len, &sim->genome);
    if (st != BIOSIM_OK) {
        biosim_sim_free(sim);
        return st;
    }

    /* max_conn = max_gen_len: worst case every gene survives culling */
    st = biosim_nnet_create(pop, max_gen_len, max_neurons, &sim->nnet);
    if (st != BIOSIM_OK) {
        biosim_sim_free(sim);
        return st;
    }

    sim->signal_len = (size_t)size_x * (size_t)size_y;
    sim->signal = (uint32_t *)calloc(sim->signal_len, sizeof(uint32_t));
    if (sim->signal == NULL) {
        biosim_sim_free(sim);
        return BIOSIM_ERR_NOMEM;
    }

    st = biosim_generation_init_random(sim);
    if (st != BIOSIM_OK) {
        biosim_sim_free(sim);
        return st;
    }

    return BIOSIM_OK;
}

void biosim_sim_free(biosim_sim_t *sim) {
    if (sim == NULL) {
        return;
    }
    biosim_nnet_free(&sim->nnet);
    biosim_genome_free(&sim->genome);
    biosim_agents_free(&sim->agents);
    biosim_grid_free(&sim->grid);
    free(sim->signal);
    sim->signal = NULL;
    sim->signal_len = 0;
    free(sim->barrier_ctrs);
    sim->barrier_ctrs = NULL;
    sim->n_barrier_ctrs = 0;
}

/* ── per-step ───────────────────────────────────────────────────────────── */

void biosim_sim_step_agent(biosim_sim_t *sim, uint32_t i) {
    float sensor_vals[BIOSIM_NUM_SENSORS];
    float action_vals[BIOSIM_NUM_ACTIONS];

    for (uint32_t s = 0; s < BIOSIM_NUM_SENSORS; s++) {
        sensor_vals[s] = biosim_sensor_eval((biosim_sensor_t)s, i, sim, sim->step);
    }

    memset(action_vals, 0, sizeof(action_vals));
    biosim_nnet_feedforward(&sim->nnet, i, sensor_vals, BIOSIM_NUM_SENSORS, action_vals,
                            BIOSIM_NUM_ACTIONS);

    sim->agents.dx_sum[i] = 0.0F;
    sim->agents.dy_sum[i] = 0.0F;

    for (uint32_t a = 0; a < BIOSIM_NUM_ACTIONS; a++) {
        biosim_action_apply((biosim_action_t)a, action_vals[a], i, sim);
    }

    /* KILL_FORWARD targets others, not self; still guard in case a prior agent
     * killed this one during action application. */
    if (!sim->agents.alive[i]) {
        return;
    }

    biosim_action_finalize_movement(i, sim);

    const int dx = (int)sim->agents.desired_x[i] - (int)sim->agents.loc_x[i];
    const int dy = (int)sim->agents.desired_y[i] - (int)sim->agents.loc_y[i];

    if (dx == 0 && dy == 0) {
        return;
    }

    biosim_coord_t target;
    target.x = sim->agents.desired_x[i];
    target.y = sim->agents.desired_y[i];

    if (biosim_grid_at(&sim->grid, target) != BIOSIM_GRID_EMPTY) {
        return;
    }

    biosim_coord_t old_loc;
    old_loc.x = sim->agents.loc_x[i];
    old_loc.y = sim->agents.loc_y[i];

    biosim_grid_set(&sim->grid, old_loc, BIOSIM_GRID_EMPTY);
    biosim_grid_set(&sim->grid, target, (uint16_t)(i + 1U));
    sim->agents.loc_x[i] = target.x;
    sim->agents.loc_y[i] = target.y;
    sim->agents.last_move_dir[i] = biosim_get_dir(dx, dy);
}

void biosim_sim_next_step(biosim_sim_t *sim) {
    for (size_t j = 0; j < sim->signal_len; j++) {
        sim->signal[j]--;
    }
    biosim_challenge_step(&sim->challenge, sim, (int)sim->step, sim->steps_per_gen);
    sim->step++;
}

/* ── per-generation ─────────────────────────────────────────────────────── */

void biosim_sim_next_generation(biosim_sim_t *sim, struct biosim_census *out) {
    const uint32_t pop = sim->agents.population;

    uint32_t *survivors = malloc(pop * sizeof(uint32_t));
    float *scores = malloc(pop * sizeof(float));
    uint32_t n_survivors = 0;
    if (survivors != NULL && scores != NULL) {
        n_survivors = biosim_generation_collect_survivors(sim, survivors, scores);
    }

    biosim_census_take(sim, survivors, n_survivors, out);

    if (n_survivors > 0) {
        (void)biosim_generation_reproduce(sim, survivors, scores, n_survivors);
    } else {
        (void)biosim_generation_init_random(sim);
    }
    free(survivors);
    free(scores);

    sim->kills = 0;
    sim->step = 0;
    sim->gen++;
}
