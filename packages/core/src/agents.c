#include "biosim/core/agents.h"
#include "biosim/core/rng.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* ── lifecycle ──────────────────────────────────────────────────────────── */

biosim_status_t biosim_agents_create(uint32_t population, biosim_agents_t *out) {
    assert(out != NULL);
    assert(population > 0);

    memset(out, 0, sizeof(*out));
    out->population = population;

/* calloc one buffer; on failure release everything allocated so far and bail */
#define ALLOC(field, type)                                                                         \
    out->field = calloc(population, sizeof(type));                                                 \
    if (!out->field) {                                                                             \
        biosim_agents_free(out);                                                                   \
        return BIOSIM_ERR_NOMEM;                                                                   \
    }

    ALLOC(loc_x, int32_t)
    ALLOC(loc_y, int32_t)
    ALLOC(birth_x, int32_t)
    ALLOC(birth_y, int32_t)
    ALLOC(alive, uint8_t)
    ALLOC(osc_period, uint16_t)
    ALLOC(responsiveness, float)
    ALLOC(long_probe_dist, uint8_t)
    ALLOC(last_move_dir, uint8_t)
    ALLOC(kill_marker, uint8_t)
    ALLOC(challenge_bits, uint32_t)
    ALLOC(rng_state, uint64_t)
    ALLOC(genome_fingerprint, uint64_t)
    ALLOC(desired_x, int32_t)
    ALLOC(desired_y, int32_t)
    ALLOC(dx_sum, float)
    ALLOC(dy_sum, float)

#undef ALLOC

    return BIOSIM_OK;
}

void biosim_agents_free(biosim_agents_t *agents) {
    if (!agents) {
        return;
    }
    free(agents->loc_x);
    free(agents->loc_y);
    free(agents->birth_x);
    free(agents->birth_y);
    free(agents->alive);
    free(agents->osc_period);
    free(agents->responsiveness);
    free(agents->long_probe_dist);
    free(agents->last_move_dir);
    free(agents->kill_marker);
    free(agents->challenge_bits);
    free(agents->rng_state);
    free(agents->genome_fingerprint);
    free(agents->desired_x);
    free(agents->desired_y);
    free(agents->dx_sum);
    free(agents->dy_sum);
    memset(agents, 0, sizeof(*agents));
}

/* ── slot initialisation ────────────────────────────────────────────────── */

void biosim_agents_init_slot(
    biosim_agents_t *agents,
    uint32_t idx,
    biosim_coord_t loc,
    uint8_t long_probe_dist,
    uint64_t rng_seed
) {
    assert(agents != NULL);
    assert(idx < agents->population);

    agents->alive[idx] = 1;
    agents->loc_x[idx] = loc.x;
    agents->loc_y[idx] = loc.y;
    agents->birth_x[idx] = loc.x;
    agents->birth_y[idx] = loc.y;
    agents->osc_period[idx] = 34;
    agents->responsiveness[idx] = 0.5F;
    agents->long_probe_dist[idx] = long_probe_dist;
    agents->last_move_dir[idx] = 0;
    agents->kill_marker[idx] = 0;
    agents->challenge_bits[idx] = 0;
    agents->rng_state[idx] = biosim_rng_seed(idx, rng_seed);
    agents->genome_fingerprint[idx] = 0;
    agents->desired_x[idx] = loc.x;
    agents->desired_y[idx] = loc.y;
}
