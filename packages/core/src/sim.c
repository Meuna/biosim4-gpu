#include "biosim/core/sim.h"
#include "biosim/core/io_catalogue.h"
#include "biosim/core/rng.h"

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
    const uint8_t long_probe_dist = sim->long_probe_dist;

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

    for (uint32_t i = 0; i < pop; i++) {
        uint64_t rng = biosim_rng_seed(i, 0);

        biosim_genome_init_slot(&sim->genome, i, max_gen_len, &rng);
        biosim_nnet_compile_slot(&sim->nnet, &sim->genome, i, BIOSIM_NUM_SENSORS,
                                 BIOSIM_NUM_ACTIONS);

        biosim_coord_t loc;
        st = biosim_grid_find_empty(&sim->grid, &rng, &loc);
        if (st != BIOSIM_OK) {
            biosim_sim_free(sim);
            return st;
        }

        biosim_agents_init_slot(&sim->agents, i, loc, long_probe_dist, 0);
        /* Overwrite the seed stored by init_slot with the already-advanced rng,
         * preserving continuity across genome init and grid placement. */
        sim->agents.rng_state[i] = rng;

        biosim_grid_set(&sim->grid, loc, (uint16_t)(i + 1U));
        sim->agents.genome_fingerprint[i] = biosim_nnet_fingerprint(&sim->nnet, i);
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
