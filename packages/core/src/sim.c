#include "biosim/core/sim.h"

#include "biosim/core/census.h"
#include "biosim/core/challenges.h"
#include "biosim/core/generation.h"
#include "biosim/core/io_eval.h"
#include "biosim/core/log.h"
#include "biosim/core/params.h"
#include "biosim/core/rng.h"
#include "biosim/core/snapshot.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* ── lifecycle ──────────────────────────────────────────────────────────── */

/* Allocate the barrier-centre array and call the placer. */
static biosim_status_t sim_place_barriers(
    biosim_sim_t *sim, const biosim_barrier_spec_t *barriers, uint32_t n_barriers
) {
    sim->barrier_ctrs = NULL;
    sim->n_barrier_ctrs = 0U;
    biosim_status_t returncode = BIOSIM_OK;
    if (n_barriers == 0U) {
        goto exit;
    }
    sim->barrier_ctrs = (biosim_coord_t *)malloc((size_t)n_barriers * sizeof(biosim_coord_t));
    if (sim->barrier_ctrs == NULL) {
        returncode = BIOSIM_ERR_NOMEM;
        goto exit;
    }
    uint64_t barrier_rng = biosim_rng_seed(0, 0);
    biosim_barriers_place(&sim->grid, barriers, n_barriers, &barrier_rng, sim->barrier_ctrs);

    sim->n_barrier_ctrs = n_barriers;

exit:
    return returncode;
}

biosim_status_t biosim_sim_create(
    biosim_sim_t *sim,
    const biosim_params_t *p,
    const biosim_challenge_spec_t *challenge,
    const biosim_barrier_spec_t *barriers,
    uint32_t n_barriers
) {
    /* alloc start here, free on exit label */
    memset(sim, 0, sizeof(*sim));

    sim->max_generations = (uint32_t)biosim_params_get_int(p, "max-generations");
    sim->population = (uint32_t)biosim_params_get_int(p, "population");
    sim->size_x = biosim_params_get_int(p, "grid-size-x");
    sim->size_y = biosim_params_get_int(p, "grid-size-y");
    sim->genome_max_len = (uint16_t)biosim_params_get_int(p, "max-genome-len");
    sim->max_neurons = (uint8_t)biosim_params_get_int(p, "max-neurons");
    sim->long_probe_dist = (uint8_t)biosim_params_get_int(p, "long-probe-dist");
    sim->steps_per_gen = (uint32_t)biosim_params_get_int(p, "steps-per-gen");
    sim->population_sensor_radius = biosim_params_get_int(p, "population-sensor-radius");
    sim->enable_kill = biosim_params_get_bool(p, "enable-kill");
    sim->mutation_rate = (float)biosim_params_get_float(p, "point-mutation-rate");
    sim->sexual_reproduction = biosim_params_get_bool(p, "sexual-reproduction");
    sim->choose_parents_by_fitness = biosim_params_get_bool(p, "choose-parents-by-fitness");
    sim->challenge = *challenge;
    sim->gen_rng = biosim_rng_seed(0U, 1U);

    biosim_status_t returncode = BIOSIM_OK;

    returncode = biosim_grid_create(sim->size_x, sim->size_y, &sim->grid);
    if (returncode != BIOSIM_OK) {
        goto exit;
    }

    returncode = biosim_agents_create(sim->population, &sim->agents);
    if (returncode != BIOSIM_OK) {
        goto exit;
    }

    returncode = biosim_genome_create(sim->population, sim->genome_max_len, &sim->genome);
    if (returncode != BIOSIM_OK) {
        goto exit;
    }

    returncode =
        biosim_nnet_create(sim->population, sim->genome_max_len, sim->max_neurons, &sim->nnet);
    if (returncode != BIOSIM_OK) {
        goto exit;
    }

    returncode = sim_place_barriers(sim, barriers, n_barriers);
    if (returncode != BIOSIM_OK) {
        goto exit;
    }

    sim->signal_len = (size_t)sim->size_x * (size_t)sim->size_y;
    sim->signal = (uint32_t *)calloc(sim->signal_len, sizeof(uint32_t));
    if (sim->signal == NULL) {
        returncode = BIOSIM_ERR_NOMEM;
        goto exit;
    }

    returncode = biosim_generation_init_random(sim);

exit:
    if (returncode != BIOSIM_OK) {
        biosim_sim_free(sim);
        BIOSIM_ERRORF("sim creation failed (%s)", biosim_strerror(returncode));
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

static void sim_grant_move(biosim_sim_t *sim, uint32_t i) {
    if (!sim->agents.alive[i]) {
        return;
    }
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
    biosim_grid_set(&sim->grid, target, i + 1U);
    sim->agents.loc_x[i] = target.x;
    sim->agents.loc_y[i] = target.y;
    sim->agents.last_move_dir[i] = biosim_get_dir(dx, dy);
}

void biosim_sim_step_agent(biosim_sim_t *sim, uint32_t i) {
    float sensor_vals[BIOSIM_NUM_SENSORS];
    float action_vals[BIOSIM_NUM_ACTIONS];

    for (uint32_t s = 0; s < BIOSIM_NUM_SENSORS; s++) {
        sensor_vals[s] = biosim_sensor_eval((biosim_sensor_t)s, i, sim);
    }

    memset(action_vals, 0, sizeof(action_vals));
    biosim_nnet_feedforward(
        &sim->nnet, i, sensor_vals, BIOSIM_NUM_SENSORS, action_vals, BIOSIM_NUM_ACTIONS
    );

    sim->agents.dx_sum[i] = 0.0F;
    sim->agents.dy_sum[i] = 0.0F;

    for (uint32_t a = 0; a < BIOSIM_NUM_ACTIONS; a++) {
        biosim_action_apply((biosim_action_t)a, action_vals[a], i, sim);
    }

    biosim_action_propose_move(i, sim);
}

void biosim_sim_next_step(biosim_sim_t *sim) {
    const uint32_t pop = sim->agents.population;

    /* Phase 1 (≈ K2): commit kills and clear grid cells. */
    for (uint32_t i = 0U; i < pop; i++) {
        if (!sim->agents.kill_marker[i]) {
            continue;
        }
        sim->agents.alive[i] = 0U;
        biosim_coord_t loc;
        loc.x = sim->agents.loc_x[i];
        loc.y = sim->agents.loc_y[i];
        biosim_grid_set(&sim->grid, loc, BIOSIM_GRID_EMPTY);
        sim->kills++;
        sim->agents.kill_marker[i] = 0U;
    }

    /* Phase 2 (≈ K3): grant movement — first-come, first-served. */
    for (uint32_t i = 0U; i < pop; i++) {
        sim_grant_move(sim, i);
    }

    /* Signal fade (≈ K4). */
    for (size_t j = 0; j < sim->signal_len; j++) {
        if (sim->signal[j] > 0) {
            sim->signal[j]--;
        }
    }

    /* Step level challenges (≈ K5). */
    biosim_challenge_step(sim);
    sim->step++;
}

/* ── per-generation ─────────────────────────────────────────────────────── */

biosim_status_t biosim_sim_next_generation(biosim_sim_t *sim, struct biosim_census *out) {
    const uint32_t pop = sim->agents.population;

    /* alloc start here, free on exit label */
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
    returncode = biosim_snapshot_session_write(sim, survivors, scores, n_survivors);
    if (returncode != BIOSIM_OK) {
        goto exit;
    }

    if (n_survivors > 0) {
        returncode = biosim_generation_reproduce(sim, survivors, scores, n_survivors);
    } else {
        returncode = biosim_generation_init_random(sim);
    }

    sim->kills = 0U;
    sim->step = 0U;
    sim->gen++;

exit:
    free(survivors);
    free(scores);
    if (returncode != BIOSIM_OK) {
        BIOSIM_ERRORF("next generation failed (%s)", biosim_strerror(returncode));
    }
    return returncode;
}
