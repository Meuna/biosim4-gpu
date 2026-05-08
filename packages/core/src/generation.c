#include "biosim/core/generation.h"

#include "biosim/core/challenges.h"
#include "biosim/core/grid.h"
#include "biosim/core/io_catalogue.h"
#include "biosim/core/log.h"
#include "biosim/core/nnet.h"
#include "biosim/core/rng.h"
#include "biosim/core/types.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* ── survivor collection ────────────────────────────────────────────────── */

uint32_t biosim_generation_collect_survivors(biosim_sim_t *sim, uint32_t *survivors,
                                             float *scores) {
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
        survivors[n] = i;
        scores[n] = r.score;
        n++;
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
    const uint16_t g_max_len = genome->max_len;
    const uint8_t long_probe_dist = sim->long_probe_dist;

    clear_agents_from_grid(sim);

    const uint64_t gen_seed = biosim_rng_next(&sim->gen_rng);

    biosim_status_t returncode = BIOSIM_OK;

    for (uint32_t i = 0; i < pop; i++) {
        uint16_t rand_len =
            (uint16_t)(1U + (uint16_t)(biosim_rng_next(&sim->gen_rng) % (uint64_t)g_max_len));
        biosim_genome_init_slot(genome, i, rand_len, &sim->gen_rng);

        returncode =
            biosim_nnet_compile_slot(nnet, genome, i, BIOSIM_NUM_SENSORS, BIOSIM_NUM_ACTIONS);
        if (returncode != BIOSIM_OK) {
            goto exit;
        }
        const uint64_t fp = biosim_nnet_fingerprint(nnet, i);

        biosim_coord_t loc;
        returncode = biosim_grid_find_empty(grid, &sim->gen_rng, &loc);
        if (returncode != BIOSIM_OK) {
            goto exit;
        }
        biosim_agents_init_slot(agents, i, loc, long_probe_dist, biosim_rng_seed(i, gen_seed));
        agents->genome_fingerprint[i] = fp;
        biosim_grid_set(grid, loc, (uint16_t)(i + 1U));
    }

exit:
    if (returncode != BIOSIM_OK) {
        BIOSIM_ERRORF("generation random init failed (%s)", biosim_strerror(returncode));
        goto exit;
    }
    return BIOSIM_OK;
}

/* ── reproduction ───────────────────────────────────────────────────────── */

/*
 * Copy survivor genome data into a compact flat array before overwriting any
 * genome slot.  This prevents parent data from being clobbered during in-place
 * reproduction.
 *
 * temp_conn / temp_wgt are strided by genome->max_len: entry s starts at
 * s * max_len.  temp_len[s] holds the active gene count for survivor s.
 */
static void snapshot_survivor_genomes(const biosim_genome_t *genome, const uint32_t *survivors,
                                      uint32_t n_survivors, uint16_t *temp_conn, int16_t *temp_wgt,
                                      uint16_t *temp_len) {
    const uint32_t pop = genome->population;
    const uint16_t g_max_len = genome->max_len;

    for (uint32_t s = 0; s < n_survivors; s++) {
        const uint32_t src = survivors[s];
        const uint16_t len = genome->len[src];
        temp_len[s] = len;
        for (uint16_t j = 0; j < len; j++) {
            temp_conn[(size_t)s * g_max_len + j] = genome->conn[(size_t)j * pop + src];
            temp_wgt[(size_t)s * g_max_len + j] = genome->wgt[(size_t)j * pop + src];
        }
    }
}

static void restore_genome_slot(biosim_genome_t *genome, uint32_t dst, const uint16_t *temp_conn,
                                const int16_t *temp_wgt, const uint16_t *temp_len,
                                uint32_t parent_s) {
    const uint32_t pop = genome->population;
    const uint16_t g_max_len = genome->max_len;
    const uint16_t len = temp_len[parent_s];

    genome->len[dst] = len;
    for (uint16_t j = 0; j < len; j++) {
        genome->conn[(size_t)j * pop + dst] = temp_conn[(size_t)parent_s * g_max_len + j];
        genome->wgt[(size_t)j * pop + dst] = temp_wgt[(size_t)parent_s * g_max_len + j];
    }
}

/*
 * Single-point crossover directly from snapshot temp buffers into a live
 * genome slot.  Mirrors biosim_genome_crossover semantics: crossover point k
 * in [0, min(len_a, len_b)]; child gets genes [0..k) from pa_s and [k..len_b)
 * from pb_s; child len = pb_s len (capped at max_len).
 */
static void crossover_from_snapshot(biosim_genome_t *genome, uint32_t dst,
                                    const uint16_t *temp_conn, const int16_t *temp_wgt,
                                    const uint16_t *temp_len, uint32_t pa_s, uint32_t pb_s,
                                    uint64_t *rng) {
    const uint32_t pop = genome->population;
    const uint16_t g_max_len = genome->max_len;
    const uint32_t len_a = temp_len[pa_s];
    const uint32_t len_b = temp_len[pb_s];
    const uint32_t min_len = len_a < len_b ? len_a : len_b;
    const uint32_t k = (uint32_t)(biosim_rng_next(rng) % ((uint64_t)min_len + 1ULL));
    const uint32_t child_len = len_b < (uint32_t)g_max_len ? len_b : (uint32_t)g_max_len;

    for (uint32_t j = 0; j < k; j++) {
        genome->conn[(size_t)j * pop + dst] = temp_conn[(size_t)pa_s * g_max_len + j];
        genome->wgt[(size_t)j * pop + dst] = temp_wgt[(size_t)pa_s * g_max_len + j];
    }
    for (uint32_t j = k; j < child_len; j++) {
        genome->conn[(size_t)j * pop + dst] = temp_conn[(size_t)pb_s * g_max_len + j];
        genome->wgt[(size_t)j * pop + dst] = temp_wgt[(size_t)pb_s * g_max_len + j];
    }
    genome->len[dst] = (uint16_t)child_len;
}

/*
 * Select one or two parent snapshot indices.
 *
 * When by_fitness && n_survivors > 1:
 *   pa drawn uniformly from [1, n-1]; pb drawn from [0, pa-1] — harmonically
 *   biased toward 0 (best score after sort).  Mirrors the reference algorithm.
 * When !by_fitness: pa and pb drawn uniformly from [0, n-1].
 * When !sexual: only pb is used by the caller; pa is still computed to keep
 *   the RNG sequence consistent when by_fitness is true.
 */
static void select_parents(uint32_t n_survivors, bool by_fitness, bool sexual, uint64_t *rng,
                           uint32_t *pa_out, uint32_t *pb_out) {
    if (by_fitness && n_survivors > 1) {
        uint32_t pa = 1U + (uint32_t)(biosim_rng_next(rng) % (uint64_t)(n_survivors - 1U));
        uint32_t pb = (uint32_t)(biosim_rng_next(rng) % (uint64_t)pa);
        *pa_out = pa;
        *pb_out = pb;
    } else {
        *pa_out = (uint32_t)(biosim_rng_next(rng) % (uint64_t)n_survivors);
        if (sexual) {
            *pb_out = (uint32_t)(biosim_rng_next(rng) % (uint64_t)n_survivors);
        } else {
            *pb_out = *pa_out;
        }
    }
}

/* Materialise a child genome from the snapshot: crossover (sexual) or copy (asexual). */
static void materialize_child(biosim_genome_t *genome, uint32_t dst, const uint16_t *temp_conn,
                              const int16_t *temp_wgt, const uint16_t *temp_len, uint32_t pa,
                              uint32_t pb, bool sexual, uint64_t *rng) {
    if (sexual) {
        crossover_from_snapshot(genome, dst, temp_conn, temp_wgt, temp_len, pa, pb, rng);
    } else {
        restore_genome_slot(genome, dst, temp_conn, temp_wgt, temp_len, pb);
    }
}

/* ── survivor sorting ───────────────────────────────────────────────────── */

typedef struct {
    uint32_t idx;
    float score;
} survivor_entry_t;

static int cmp_survivor_desc(const void *a, const void *b) {
    const survivor_entry_t *ea = (const survivor_entry_t *)a;
    const survivor_entry_t *eb = (const survivor_entry_t *)b;
    if (eb->score > ea->score) {
        return 1;
    }
    if (eb->score < ea->score) {
        return -1;
    }
    return 0;
}

/*
 * Sort survivors[] and scores[] together by score descending so that index 0
 * holds the highest-scoring parent.
 */
static biosim_status_t sort_survivors_by_score(uint32_t *survivors, float *scores, uint32_t n) {
    /* alloc start here, free on exit label */
    survivor_entry_t *tmp = NULL;
    biosim_status_t returncode = BIOSIM_OK;

    tmp = malloc((size_t)n * sizeof(survivor_entry_t));
    if (!tmp) {
        returncode = BIOSIM_ERR_NOMEM;
        goto exit;
    }
    for (uint32_t i = 0; i < n; i++) {
        tmp[i].idx = survivors[i];
        tmp[i].score = scores[i];
    }
    qsort(tmp, (size_t)n, sizeof(survivor_entry_t), cmp_survivor_desc);
    for (uint32_t i = 0; i < n; i++) {
        survivors[i] = tmp[i].idx;
        scores[i] = tmp[i].score;
    }
exit:
    free(tmp);
    return returncode;
}

/* ── main reproduce entry point ─────────────────────────────────────────── */

/* Place one agent slot: select parents, build genome, compile nnet, find cell. */
static biosim_status_t reproduce_one_agent(biosim_sim_t *sim, uint32_t i, const uint16_t *temp_conn,
                                           const int16_t *temp_wgt, const uint16_t *temp_len,
                                           uint32_t n_survivors, uint64_t gen_seed) {
    biosim_genome_t *genome = &sim->genome;
    biosim_nnet_t *nnet = &sim->nnet;
    biosim_agents_t *agents = &sim->agents;
    biosim_grid_t *grid = &sim->grid;

    uint32_t pa;
    uint32_t pb;
    select_parents(n_survivors, sim->choose_parents_by_fitness, sim->sexual_reproduction,
                   &sim->gen_rng, &pa, &pb);
    materialize_child(genome, i, temp_conn, temp_wgt, temp_len, pa, pb, sim->sexual_reproduction,
                      &sim->gen_rng);
    biosim_genome_mutate(genome, i, sim->mutation_rate, &sim->gen_rng);

    biosim_status_t returncode =
        biosim_nnet_compile_slot(nnet, genome, i, BIOSIM_NUM_SENSORS, BIOSIM_NUM_ACTIONS);
    if (returncode != BIOSIM_OK) {
        goto exit;
    }
    const uint64_t fp = biosim_nnet_fingerprint(nnet, i);

    biosim_coord_t loc;
    returncode = biosim_grid_find_empty(grid, &sim->gen_rng, &loc);
    if (returncode != BIOSIM_OK) {
        goto exit;
    }
    biosim_agents_init_slot(agents, i, loc, sim->long_probe_dist, biosim_rng_seed(i, gen_seed));
    agents->genome_fingerprint[i] = fp;
    biosim_grid_set(grid, loc, (uint16_t)(i + 1U));

exit:
    if (returncode != BIOSIM_OK) {
        BIOSIM_ERRORF("failed to reproduce agent %u (%s)", i, biosim_strerror(returncode));
    }
    return returncode;
}

biosim_status_t biosim_generation_reproduce(biosim_sim_t *sim, uint32_t *survivors, float *scores,
                                            uint32_t n_survivors) {
    assert(n_survivors > 0);

    biosim_genome_t *genome = &sim->genome;

    const uint32_t pop = sim->agents.population;
    const uint16_t g_max_len = genome->max_len;
    const bool by_fitness = sim->choose_parents_by_fitness;

    /* alloc start here, free on exit label */
    uint16_t *temp_conn = NULL;
    int16_t *temp_wgt = NULL;
    uint16_t *temp_len = NULL;
    biosim_status_t returncode = BIOSIM_OK;

    /* Sort temp buffers snapshot so that they are already in score order */
    if (by_fitness && n_survivors > 1 && scores != NULL) {
        returncode = sort_survivors_by_score(survivors, scores, n_survivors);
        if (returncode != BIOSIM_OK) {
            goto exit;
        }
    }

    clear_agents_from_grid(sim);

    /* snapshot survivor genomes before any slot is overwritten */
    temp_conn = malloc((size_t)n_survivors * g_max_len * sizeof(uint16_t));
    temp_wgt = malloc((size_t)n_survivors * g_max_len * sizeof(int16_t));
    temp_len = malloc((size_t)n_survivors * sizeof(uint16_t));

    if (temp_conn == NULL || temp_wgt == NULL || temp_len == NULL) {
        returncode = BIOSIM_ERR_NOMEM;
        goto exit;
    }

    snapshot_survivor_genomes(genome, survivors, n_survivors, temp_conn, temp_wgt, temp_len);

    const uint64_t gen_seed = biosim_rng_next(&sim->gen_rng);

    for (uint32_t i = 0; i < pop; i++) {
        biosim_status_t st =
            reproduce_one_agent(sim, i, temp_conn, temp_wgt, temp_len, n_survivors, gen_seed);
        if (st != BIOSIM_OK) {
            returncode = st;
            goto exit;
        }
    }

exit:
    free(temp_conn);
    free(temp_wgt);
    free(temp_len);
    if (returncode != BIOSIM_OK) {
        BIOSIM_ERRORF("generation reproduce failed (%s)", biosim_strerror(returncode));
    }
    return returncode;
}
