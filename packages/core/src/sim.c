#include "biosim/core/sim.h"

#include "biosim/core/census.h"
#include "biosim/core/challenges.h"
#include "biosim/core/generation.h"
#include "biosim/core/io_catalogue.h"
#include "biosim/core/rng.h"
#include "biosim/core/snapshot.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* ── lifecycle ──────────────────────────────────────────────────────────── */

biosim_status_t biosim_sim_create(biosim_sim_t *sim, const biosim_barrier_spec_t *barriers,
                                  int n_barriers) {
    /* alloc start here, freed on exit label */
    const uint32_t pop = sim->population;
    const int16_t size_x = sim->size_x;
    const int16_t size_y = sim->size_y;
    const uint16_t genome_max_len = sim->genome_max_len;
    const uint8_t max_neurons = sim->max_neurons;
    biosim_status_t returncode = BIOSIM_OK;

    sim->kills = 0;

    returncode = biosim_grid_create(size_x, size_y, &sim->grid);
    if (returncode != BIOSIM_OK) {
        goto exit;
    }

    sim->barrier_ctrs = NULL;
    sim->n_barrier_ctrs = 0;
    if (n_barriers > 0) {
        sim->barrier_ctrs = (biosim_coord_t *)malloc((size_t)n_barriers * sizeof(biosim_coord_t));
        if (sim->barrier_ctrs == NULL) {
            returncode = BIOSIM_ERR_NOMEM;
            goto exit;
        }
        uint64_t barrier_rng = biosim_rng_seed(0, 0);
        returncode = biosim_barriers_place(&sim->grid, barriers, n_barriers, &barrier_rng,
                                           sim->barrier_ctrs);
        if (returncode != BIOSIM_OK) {
            goto exit;
        }
        sim->n_barrier_ctrs = n_barriers;
    }

    returncode = biosim_agents_create(pop, &sim->agents);
    if (returncode != BIOSIM_OK) {
        goto exit;
    }

    returncode = biosim_genome_create(pop, genome_max_len, &sim->genome);
    if (returncode != BIOSIM_OK) {
        goto exit;
    }

    /* max_conn = genome_max_len: worst case every gene survives culling */
    returncode = biosim_nnet_create(pop, genome_max_len, max_neurons, &sim->nnet);
    if (returncode != BIOSIM_OK) {
        goto exit;
    }

    sim->signal_len = (size_t)size_x * (size_t)size_y;
    sim->signal = (uint32_t *)calloc(sim->signal_len, sizeof(uint32_t));
    if (sim->signal == NULL) {
        returncode = BIOSIM_ERR_NOMEM;
        goto exit;
    }

    returncode = biosim_generation_init_random(sim);

exit:
    if (returncode != BIOSIM_OK) {
        biosim_sim_free(sim);
    }
    return returncode;
}

void biosim_sim_free(biosim_sim_t *sim) {
    if (sim == NULL) {
        return;
    }
    biosim_snapshot_session_close(sim);
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

biosim_status_t biosim_sim_next_generation(biosim_sim_t *sim, struct biosim_census *out) {
    const uint32_t pop = sim->agents.population;

    /* alloc start here, freed on exit label */
    uint32_t *survivors = NULL;
    float *scores = NULL;
    biosim_status_t returncode = BIOSIM_OK;

    survivors = malloc(pop * sizeof(uint32_t));
    scores = malloc(pop * sizeof(float));
    if (survivors == NULL || scores == NULL) {
        returncode = BIOSIM_ERR_NOMEM;
        goto exit;
    }

    uint32_t n_survivors = biosim_generation_collect_survivors(sim, survivors, scores);
    biosim_census_take(sim, survivors, n_survivors, out);
    biosim_snapshot_session_write(sim, survivors, n_survivors);

    if (n_survivors > 0) {
        returncode = biosim_generation_reproduce(sim, survivors, scores, n_survivors);
    } else {
        returncode = biosim_generation_init_random(sim);
    }
    if (returncode != BIOSIM_OK) {
        goto exit;
    }

    sim->kills = 0U;
    sim->step = 0U;
    sim->gen++;

exit:
    free(survivors);
    free(scores);
    return returncode;
}
