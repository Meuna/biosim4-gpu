#include "biosim/core/generation.h"
#include "biosim/core/snapshot.h"

#include "biosim/core/challenges.h"
#include "biosim/core/grid.h"
#include "biosim/core/grid_defs.h"
#include "biosim/core/io_eval.h"
#include "biosim/core/log.h"
#include "biosim/core/nnet.h"
#include "biosim/core/rng.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* ── survivor collection ────────────────────────────────────────────────── */

biosim_status_t biosim_generation_collect_survivors(
    biosim_sim_t *sim, biosim_survivor_snap_t *snap
) {
    const uint32_t pop = sim->agents.population;
    const biosim_genome_t *genome = &sim->genome;
    const uint16_t g_max_len = genome->max_len;

    uint32_t *temp_idx = NULL;
    float *temp_scores = NULL;
    biosim_status_t returncode = BIOSIM_OK;

    snap->gen = sim->gen;
    snap->gen_rng = sim->gen_rng;

    temp_idx = malloc((size_t)pop * sizeof(uint32_t));
    if (temp_idx == NULL) {
        returncode = BIOSIM_ERR_NOMEM;
        goto exit;
    }
    temp_scores = malloc((size_t)pop * sizeof(float));
    if (temp_scores == NULL) {
        returncode = BIOSIM_ERR_NOMEM;
        goto exit;
    }

    uint32_t n = 0U;
    for (uint32_t i = 0U; i < pop; i++) {
        if (!sim->agents.alive[i]) {
            continue;
        }
        biosim_challenge_result_t r = biosim_challenge_eval(&sim->challenge, i, sim);
        if (!r.passed) {
            continue;
        }
        temp_idx[n] = i;
        temp_scores[n] = r.score;
        n++;
    }

    returncode = biosim_survivor_snap_grow(snap, n, g_max_len);
    if (returncode != BIOSIM_OK) {
        goto exit;
    }

    for (uint32_t s = 0U; s < n; s++) {
        const uint32_t src = temp_idx[s];
        const uint16_t len = genome->len[src];
        snap->len[s] = len;
        snap->scores[s] = temp_scores[s];
        for (uint16_t j = 0U; j < len; j++) {
            snap->conn[(size_t)s * snap->stride_cap + j] = genome->conn[(size_t)j * pop + src];
            snap->wgt[(size_t)s * snap->stride_cap + j] = genome->wgt[(size_t)j * pop + src];
        }
    }
    snap->count = n;

exit:
    free(temp_idx);
    free(temp_scores);
    if (returncode != BIOSIM_OK) {
        BIOSIM_ERRORF("collect survivors failed (%s)", biosim_strerror(returncode));
    }
    return returncode;
}

/* ── random initialisation ─────────────────────────────────────────────── */

/*
 * Clear all non-barrier cells and zero the signal layer.
 * Called at every generation boundary by both init_random and breed.
 */
static void clear_agents_from_grid(biosim_sim_t *sim) {
    biosim_grid_t *grid = &sim->grid;
    for (int32_t y = 0; y < grid->size_y; y++) {
        for (int32_t x = 0; x < grid->size_x; x++) {
            uint32_t *cell = &grid->cells[y * grid->size_x + x];
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
    const uint8_t los_range = sim->los_range;

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
        biosim_agents_init_slot(agents, i, loc, los_range, biosim_rng_seed(i, gen_seed));
        agents->genome_fingerprint[i] = fp;
        biosim_grid_set(grid, loc, i + 1U);
    }

exit:
    if (returncode != BIOSIM_OK) {
        BIOSIM_ERRORF("generation random init failed (%s)", biosim_strerror(returncode));
    }
    return returncode;
}

/* ── reproduction ───────────────────────────────────────────────────────── */

static void restore_genome_slot(
    biosim_genome_t *genome, uint32_t dst, const biosim_survivor_snap_t *snap, uint32_t snap_s
) {
    const uint32_t pop = genome->population;
    const uint16_t len = snap->len[snap_s];
    const uint16_t stride = snap->stride_cap;

    genome->len[dst] = len;
    for (uint16_t j = 0U; j < len; j++) {
        genome->conn[(size_t)j * pop + dst] = snap->conn[(size_t)snap_s * stride + j];
        genome->wgt[(size_t)j * pop + dst] = snap->wgt[(size_t)snap_s * stride + j];
    }
}

/*
 * Single-point crossover from snap rows snap_pa and snap_pb into a live
 * genome slot.  Mirrors biosim_genome_crossover semantics: crossover point k
 * in [0, min(len_a, len_b)]; child gets genes [0..k) from pa and [k..len_b)
 * from pb; child len = pb len (capped at max_len).
 */
static void crossover_from_snapshot(
    biosim_genome_t *genome,
    uint32_t dst,
    const biosim_survivor_snap_t *snap,
    uint32_t snap_pa,
    uint32_t snap_pb,
    uint64_t *rng
) {
    const uint32_t pop = genome->population;
    const uint16_t stride = snap->stride_cap;
    const uint32_t len_a = snap->len[snap_pa];
    const uint32_t len_b = snap->len[snap_pb];
    const uint32_t min_len = len_a < len_b ? len_a : len_b;
    const uint32_t k = (uint32_t)(biosim_rng_next(rng) % ((uint64_t)min_len + 1ULL));
    const uint32_t max_len = (uint32_t)genome->max_len;
    const uint32_t child_len = len_b < max_len ? len_b : max_len;

    for (uint32_t j = 0U; j < k; j++) {
        genome->conn[(size_t)j * pop + dst] = snap->conn[(size_t)snap_pa * stride + j];
        genome->wgt[(size_t)j * pop + dst] = snap->wgt[(size_t)snap_pa * stride + j];
    }
    for (uint32_t j = k; j < child_len; j++) {
        genome->conn[(size_t)j * pop + dst] = snap->conn[(size_t)snap_pb * stride + j];
        genome->wgt[(size_t)j * pop + dst] = snap->wgt[(size_t)snap_pb * stride + j];
    }
    genome->len[dst] = (uint16_t)child_len;
}

/*
 * Select one or two parent indices (into snap_order[]).
 *
 * When by_fitness && n > 1:
 *   pa drawn uniformly from [1, n-1]; pb drawn from [0, pa-1] — harmonically
 *   biased toward 0 (best score after sort).  Mirrors the reference algorithm.
 * When !by_fitness: pa and pb drawn uniformly from [0, n-1].
 * When !sexual: only pb is used by the caller; pa is still computed to keep
 *   the RNG sequence consistent when by_fitness is true.
 */
static void select_parents(
    uint32_t n, bool by_fitness, bool sexual, uint64_t *rng, uint32_t *pa_out, uint32_t *pb_out
) {
    if (by_fitness && n > 1U) {
        uint32_t pa = 1U + (uint32_t)(biosim_rng_next(rng) % (uint64_t)(n - 1U));
        uint32_t pb = (uint32_t)(biosim_rng_next(rng) % (uint64_t)pa);
        *pa_out = pa;
        *pb_out = pb;
    } else {
        *pa_out = (uint32_t)(biosim_rng_next(rng) % (uint64_t)n);
        if (sexual) {
            *pb_out = (uint32_t)(biosim_rng_next(rng) % (uint64_t)n);
        } else {
            *pb_out = *pa_out;
        }
    }
}

/* Materialise a child genome from snap rows: crossover (sexual) or copy (asexual). */
static void materialize_child(
    biosim_genome_t *genome,
    uint32_t dst,
    const biosim_survivor_snap_t *snap,
    const uint32_t *snap_order,
    uint32_t pa,
    uint32_t pb,
    bool sexual,
    uint64_t *rng
) {
    if (sexual) {
        crossover_from_snapshot(genome, dst, snap, snap_order[pa], snap_order[pb], rng);
    } else {
        restore_genome_slot(genome, dst, snap, snap_order[pb]);
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
 * Sort snap_order[0..n-1] by snap->scores[snap_order[i]] descending so that
 * snap_order[0] points to the highest-scoring survivor.
 * snap itself is not modified.
 */
static biosim_status_t sort_snap_order_by_score(
    const float *scores, uint32_t n, uint32_t *snap_order
) {
    survivor_entry_t *tmp = NULL;
    biosim_status_t returncode = BIOSIM_OK;

    tmp = malloc((size_t)n * sizeof(survivor_entry_t));
    if (tmp == NULL) {
        returncode = BIOSIM_ERR_NOMEM;
        goto exit;
    }
    for (uint32_t i = 0U; i < n; i++) {
        tmp[i].idx = snap_order[i];
        tmp[i].score = scores[snap_order[i]];
    }
    qsort(tmp, (size_t)n, sizeof(survivor_entry_t), cmp_survivor_desc);
    for (uint32_t i = 0U; i < n; i++) {
        snap_order[i] = tmp[i].idx;
    }
exit:
    free(tmp);
    return returncode;
}

/* ── main breed entry point ─────────────────────────────────────────────── */

/* Place one agent slot: select parents, build genome, compile nnet, find cell. */
static biosim_status_t reproduce_one_agent(
    biosim_sim_t *sim,
    uint32_t i,
    const biosim_survivor_snap_t *snap,
    const uint32_t *snap_order,
    uint32_t n_survivors,
    uint64_t gen_seed
) {
    biosim_genome_t *genome = &sim->genome;
    biosim_nnet_t *nnet = &sim->nnet;
    biosim_agents_t *agents = &sim->agents;
    biosim_grid_t *grid = &sim->grid;

    uint32_t pa;
    uint32_t pb;
    select_parents(
        n_survivors,
        sim->choose_parents_by_fitness,
        sim->sexual_reproduction,
        &sim->gen_rng,
        &pa,
        &pb
    );
    materialize_child(genome, i, snap, snap_order, pa, pb, sim->sexual_reproduction, &sim->gen_rng);
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
    biosim_agents_init_slot(agents, i, loc, sim->los_range, biosim_rng_seed(i, gen_seed));
    agents->genome_fingerprint[i] = fp;
    biosim_grid_set(grid, loc, (uint16_t)(i + 1U));

exit:
    if (returncode != BIOSIM_OK) {
        BIOSIM_ERRORF("failed to breed agent %u (%s)", i, biosim_strerror(returncode));
    }
    return returncode;
}

biosim_status_t biosim_generation_breed(biosim_sim_t *sim, const biosim_survivor_snap_t *snap) {
    assert(snap->count > 0U);

    const uint32_t n_survivors = snap->count;
    const bool by_fitness = sim->choose_parents_by_fitness;
    const uint32_t pop = sim->agents.population;

    /* Reject survivors whose genomes exceed the live genome capacity before
     * touching the grid: restore_genome_slot would otherwise write past
     * genome->conn/wgt. This is the hard net behind callers' compatibility
     * checks (snapshot.c check_compat for the CLI, the webapp UX gate); a direct
     * load of an oversized snapshot lands here instead of overflowing. */
    for (uint32_t s = 0U; s < n_survivors; s++) {
        if (snap->len[s] > sim->genome.max_len) {
            BIOSIM_ERRORF(
                "survivor genome length %u exceeds genome max length %u",
                (unsigned)snap->len[s],
                (unsigned)sim->genome.max_len
            );
            return BIOSIM_ERR_INVALID;
        }
    }

    uint32_t *snap_order = NULL;
    biosim_status_t returncode = BIOSIM_OK;

    snap_order = malloc((size_t)n_survivors * sizeof(uint32_t));
    if (snap_order == NULL) {
        returncode = BIOSIM_ERR_NOMEM;
        goto exit;
    }
    for (uint32_t s = 0U; s < n_survivors; s++) {
        snap_order[s] = s;
    }

    if (by_fitness && n_survivors > 1U) {
        returncode = sort_snap_order_by_score(snap->scores, n_survivors, snap_order);
        if (returncode != BIOSIM_OK) {
            goto exit;
        }
    }

    clear_agents_from_grid(sim);

    const uint64_t gen_seed = biosim_rng_next(&sim->gen_rng);

    for (uint32_t i = 0U; i < pop; i++) {
        biosim_status_t st = reproduce_one_agent(sim, i, snap, snap_order, n_survivors, gen_seed);
        if (st != BIOSIM_OK) {
            returncode = st;
            goto exit;
        }
    }

exit:
    free(snap_order);
    if (returncode != BIOSIM_OK) {
        BIOSIM_ERRORF("generation breed failed (%s)", biosim_strerror(returncode));
    }
    return returncode;
}

/* ── spawn ──────────────────────────────────────────────────────────────── */

biosim_status_t biosim_generation_spawn(biosim_sim_t *sim, biosim_survivor_snap_t *snap) {
    if (snap->count > 0U) {
        return biosim_generation_breed(sim, snap);
    }
    return biosim_generation_init_random(sim);
}
