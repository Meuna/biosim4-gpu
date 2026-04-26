#include "biosim/core/context.h"
#include "biosim/core/io_catalogue.h"
#include "biosim/core/rng.h"

#include <stdlib.h>
#include <string.h>

/* ── lifecycle ──────────────────────────────────────────────────────────── */

biosim_status_t biosim_context_create(biosim_context_t *ctx, const biosim_barrier_spec_t *barriers,
                                      int n_barriers) {
    biosim_status_t st;

    const uint32_t pop = ctx->population;
    const int16_t size_x = ctx->size_x;
    const int16_t size_y = ctx->size_y;
    const uint16_t max_gen_len = ctx->max_gen_len;
    const uint8_t max_neurons = ctx->max_neurons;
    const uint8_t long_probe_dist = ctx->long_probe_dist;

    ctx->kills = 0;

    st = biosim_grid_create(size_x, size_y, &ctx->grid);
    if (st != BIOSIM_OK) {
        biosim_context_free(ctx);
        return st;
    }

    ctx->barrier_ctrs = NULL;
    ctx->n_barrier_ctrs = 0;
    if (n_barriers > 0) {
        ctx->barrier_ctrs = (biosim_coord_t *)malloc((size_t)n_barriers * sizeof(biosim_coord_t));
        if (ctx->barrier_ctrs == NULL) {
            biosim_context_free(ctx);
            return BIOSIM_ERR_NOMEM;
        }
        uint64_t barrier_rng = biosim_rng_seed(0, 0);
        st = biosim_barriers_place(&ctx->grid, barriers, n_barriers, &barrier_rng,
                                   ctx->barrier_ctrs);
        if (st != BIOSIM_OK) {
            biosim_context_free(ctx);
            return st;
        }
        ctx->n_barrier_ctrs = n_barriers;
    }

    st = biosim_agents_create(pop, &ctx->agents);
    if (st != BIOSIM_OK) {
        biosim_context_free(ctx);
        return st;
    }

    st = biosim_genome_create(pop, max_gen_len, &ctx->genome);
    if (st != BIOSIM_OK) {
        biosim_context_free(ctx);
        return st;
    }

    /* max_conn = max_gen_len: worst case every gene survives culling */
    st = biosim_nnet_create(pop, max_gen_len, max_neurons, &ctx->nnet);
    if (st != BIOSIM_OK) {
        biosim_context_free(ctx);
        return st;
    }

    ctx->signal_len = (size_t)size_x * (size_t)size_y;
    ctx->signal = (uint32_t *)calloc(ctx->signal_len, sizeof(uint32_t));
    if (ctx->signal == NULL) {
        biosim_context_free(ctx);
        return BIOSIM_ERR_NOMEM;
    }

    for (uint32_t i = 0; i < pop; i++) {
        uint64_t rng = biosim_rng_seed(i, 0);

        biosim_genome_init_slot(&ctx->genome, i, max_gen_len, &rng);
        biosim_nnet_compile_slot(&ctx->nnet, &ctx->genome, i, BIOSIM_NUM_SENSORS,
                                 BIOSIM_NUM_ACTIONS);

        biosim_coord_t loc;
        st = biosim_grid_find_empty(&ctx->grid, &rng, &loc);
        if (st != BIOSIM_OK) {
            biosim_context_free(ctx);
            return st;
        }

        biosim_agents_init_slot(&ctx->agents, i, loc, long_probe_dist, 0);
        /* Overwrite the seed stored by init_slot with the already-advanced rng,
         * preserving continuity across genome init and grid placement. */
        ctx->agents.rng_state[i] = rng;

        biosim_grid_set(&ctx->grid, loc, (uint16_t)(i + 1U));
        ctx->agents.genome_fingerprint[i] = biosim_nnet_fingerprint(&ctx->nnet, i);
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
    free(ctx->barrier_ctrs);
    ctx->barrier_ctrs = NULL;
    ctx->n_barrier_ctrs = 0;
}
