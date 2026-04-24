#include "biosim/core/context.h"
#include "biosim/core/barriers.h"
#include "biosim/core/io_catalogue.h"
#include "biosim/core/rng.h"
#include "biosim/core/status.h"

#include <stdlib.h>

/* ── lifecycle ──────────────────────────────────────────────────────────── */

biosim_status_t biosim_context_create(uint32_t pop, int16_t size_x, int16_t size_y,
                                      int steps_per_gen, uint16_t max_gen_len, uint8_t max_neurons,
                                      uint8_t long_probe_dist, int pop_sensor_radius,
                                      const biosim_barrier_spec_t *barriers, int n_barriers,
                                      biosim_context_t *out) {
    out->steps_per_gen = steps_per_gen;
    out->population_sensor_radius = pop_sensor_radius;

    biosim_status_t st;

    st = biosim_grid_create(size_x, size_y, &out->grid);
    if (st != BIOSIM_OK) {
        biosim_context_free(out);
        return st;
    }

    if (n_barriers > 0) {
        uint64_t barrier_rng = biosim_rng_seed(0, 0);
        biosim_barriers_place(&out->grid, barriers, n_barriers, &barrier_rng);
    }

    st = biosim_agents_create(pop, &out->agents);
    if (st != BIOSIM_OK) {
        biosim_context_free(out);
        return st;
    }

    st = biosim_genome_create(pop, max_gen_len, &out->genome);
    if (st != BIOSIM_OK) {
        biosim_context_free(out);
        return st;
    }

    /* max_conn = max_gen_len: worst case every gene survives culling */
    st = biosim_nnet_create(pop, max_gen_len, max_neurons, &out->nnet);
    if (st != BIOSIM_OK) {
        biosim_context_free(out);
        return st;
    }

    out->signal_len = (size_t)size_x * (size_t)size_y;
    out->signal = (uint32_t *)calloc(out->signal_len, sizeof(uint32_t));
    if (out->signal == NULL) {
        biosim_context_free(out);
        return BIOSIM_ERR_NOMEM;
    }

    for (uint32_t i = 0; i < pop; i++) {
        uint64_t rng = biosim_rng_seed(i, 0);

        biosim_genome_init_slot(&out->genome, i, max_gen_len, &rng);
        biosim_nnet_compile_slot(&out->nnet, &out->genome, i, BIOSIM_NUM_SENSORS,
                                 BIOSIM_NUM_ACTIONS);

        biosim_coord_t loc;
        st = biosim_grid_find_empty(&out->grid, &rng, &loc);
        if (st != BIOSIM_OK) {
            biosim_context_free(out);
            return st;
        }

        biosim_agents_init_slot(&out->agents, i, loc, long_probe_dist, 0);
        /* Overwrite the seed stored by init_slot with the already-advanced rng,
         * preserving continuity across genome init and grid placement. */
        out->agents.rng_state[i] = rng;

        biosim_grid_set(&out->grid, loc, (uint16_t)(i + 1U));
        out->agents.genome_fingerprint[i] = biosim_nnet_fingerprint(&out->nnet, i);
    }

    return BIOSIM_OK;
}

void biosim_context_free(biosim_context_t *ctx) {
    if (ctx == NULL) {
        return;
    }
    biosim_nnet_free(&ctx->nnet);
    biosim_genome_free(&ctx->genome);
    biosim_agents_free(&ctx->agents);
    biosim_grid_free(&ctx->grid);
    free(ctx->signal);
    ctx->signal = NULL;
    ctx->signal_len = 0;
}
