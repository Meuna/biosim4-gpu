#include "biosim/core/generation.h"

#include "biosim/core/challenges.h"
#include "biosim/core/grid.h"
#include "biosim/core/io_catalogue.h"
#include "biosim/core/nnet.h"
#include "biosim/core/rng.h"
#include "biosim/core/types.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* ── survivor collection ────────────────────────────────────────────────── */

uint32_t biosim_generation_collect_survivors(biosim_sim_t *sim, uint32_t *survivors) {
    const uint32_t pop = sim->agents.population;
    uint32_t n = 0;

    for (uint32_t i = 0; i < pop; i++) {
        if (!sim->agents.alive[i]) {
            continue;
        }
        biosim_challenge_result_t r = biosim_challenge_eval(&sim->challenge, i, sim);
        if (!r.passed) {
            continue;
        }
        survivors[n++] = i;
    }

    return n;
}

/* ── random initialisation ─────────────────────────────────────────────── */

/*
 * Clear all non-barrier cells and zero the signal layer.
 * Called at every generation boundary by both init_random and reproduce.
 */
static void clear_agents_from_grid(biosim_sim_t *sim) {
    biosim_grid_t *grid = &sim->grid;
    for (int y = 0; y < (int)grid->size_y; y++) {
        for (int x = 0; x < (int)grid->size_x; x++) {
            uint16_t *cell = &grid->cells[y * grid->size_x + x];
            if (*cell != BIOSIM_GRID_BARRIER) {
                *cell = BIOSIM_GRID_EMPTY;
            }
        }
    }
    memset(sim->signal, 0, sim->signal_len * sizeof(uint32_t));
}

biosim_status_t biosim_generation_init_random(biosim_sim_t *sim) {
    biosim_genome_t *genome = &sim->genome;
    biosim_nnet_t *nnet = &sim->nnet;
    biosim_agents_t *agents = &sim->agents;
    biosim_grid_t *grid = &sim->grid;

    const uint32_t pop = agents->population;
    const uint16_t max_len = genome->max_length;
    const uint8_t long_probe_dist = sim->long_probe_dist;

    clear_agents_from_grid(sim);

    const uint64_t gen_seed = biosim_rng_next(&sim->gen_rng);

    for (uint32_t i = 0; i < pop; i++) {
        uint16_t rand_len =
            (uint16_t)(1U + (uint16_t)(biosim_rng_next(&sim->gen_rng) % (uint64_t)max_len));
        biosim_genome_init_slot(genome, i, rand_len, &sim->gen_rng);

        biosim_nnet_compile_slot(nnet, genome, i, BIOSIM_NUM_SENSORS, BIOSIM_NUM_ACTIONS);
        const uint64_t fp = biosim_nnet_fingerprint(nnet, i);

        biosim_coord_t loc;
        biosim_status_t st = biosim_grid_find_empty(grid, &sim->gen_rng, &loc);
        if (st != BIOSIM_OK) {
            return st;
        }
        biosim_agents_init_slot(agents, i, loc, long_probe_dist, biosim_rng_seed(i, gen_seed));
        agents->genome_fingerprint[i] = fp;
        biosim_grid_set(grid, loc, (uint16_t)(i + 1U));
    }

    return BIOSIM_OK;
}

/* ── reproduction ───────────────────────────────────────────────────────── */

/*
 * Copy survivor genome data into a compact flat array before overwriting any
 * genome slot.  This prevents parent data from being clobbered during in-place
 * reproduction.
 *
 * temp_conn / temp_wgt are strided by genome->max_length: entry s starts at
 * s * max_length.  temp_len[s] holds the active gene count for survivor s.
 */
static void snapshot_survivor_genomes(const biosim_genome_t *genome, const uint32_t *survivors,
                                      uint32_t n_survivors, uint16_t *temp_conn, int16_t *temp_wgt,
                                      uint16_t *temp_len) {
    const uint32_t pop = genome->population;
    const uint16_t max_len = genome->max_length;

    for (uint32_t s = 0; s < n_survivors; s++) {
        const uint32_t src = survivors[s];
        const uint16_t len = genome->length[src];
        temp_len[s] = len;
        for (uint16_t j = 0; j < len; j++) {
            temp_conn[(size_t)s * max_len + j] = genome->conn[(size_t)j * pop + src];
            temp_wgt[(size_t)s * max_len + j] = genome->wgt[(size_t)j * pop + src];
        }
    }
}

static void restore_genome_slot(biosim_genome_t *genome, uint32_t dst, const uint16_t *temp_conn,
                                const int16_t *temp_wgt, const uint16_t *temp_len,
                                uint32_t parent_s) {
    const uint32_t pop = genome->population;
    const uint16_t max_len = genome->max_length;
    const uint16_t len = temp_len[parent_s];

    genome->length[dst] = len;
    for (uint16_t j = 0; j < len; j++) {
        genome->conn[(size_t)j * pop + dst] = temp_conn[(size_t)parent_s * max_len + j];
        genome->wgt[(size_t)j * pop + dst] = temp_wgt[(size_t)parent_s * max_len + j];
    }
}

biosim_status_t biosim_generation_reproduce(biosim_sim_t *sim, const uint32_t *survivors,
                                            uint32_t n_survivors) {
    assert(n_survivors > 0);

    biosim_genome_t *genome = &sim->genome;
    biosim_nnet_t *nnet = &sim->nnet;
    biosim_agents_t *agents = &sim->agents;
    biosim_grid_t *grid = &sim->grid;

    const uint32_t pop = agents->population;
    const uint16_t max_len = genome->max_length;
    const uint8_t long_probe_dist = sim->long_probe_dist;

    clear_agents_from_grid(sim);

    /* snapshot survivor genomes before any slot is overwritten */
    uint16_t *temp_conn = malloc((size_t)n_survivors * max_len * sizeof(uint16_t));
    int16_t *temp_wgt = malloc((size_t)n_survivors * max_len * sizeof(int16_t));
    uint16_t *temp_len = malloc((size_t)n_survivors * sizeof(uint16_t));

    if (temp_conn == NULL || temp_wgt == NULL || temp_len == NULL) {
        free(temp_conn);
        free(temp_wgt);
        free(temp_len);
        return biosim_generation_init_random(sim);
    }

    snapshot_survivor_genomes(genome, survivors, n_survivors, temp_conn, temp_wgt, temp_len);

    const uint64_t gen_seed = biosim_rng_next(&sim->gen_rng);

    for (uint32_t i = 0; i < pop; i++) {
        uint32_t parent_s = (uint32_t)(biosim_rng_next(&sim->gen_rng) % (uint64_t)n_survivors);
        restore_genome_slot(genome, i, temp_conn, temp_wgt, temp_len, parent_s);
        biosim_genome_mutate(genome, i, sim->mutation_rate, &sim->gen_rng);

        biosim_nnet_compile_slot(nnet, genome, i, BIOSIM_NUM_SENSORS, BIOSIM_NUM_ACTIONS);
        const uint64_t fp = biosim_nnet_fingerprint(nnet, i);

        biosim_coord_t loc;
        (void)biosim_grid_find_empty(grid, &sim->gen_rng, &loc);
        biosim_agents_init_slot(agents, i, loc, long_probe_dist, biosim_rng_seed(i, gen_seed));
        agents->genome_fingerprint[i] = fp;
        biosim_grid_set(grid, loc, (uint16_t)(i + 1U));
    }

    free(temp_conn);
    free(temp_wgt);
    free(temp_len);
    return BIOSIM_OK;
}
