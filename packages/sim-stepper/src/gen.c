#include "biosim/stepper/gen.h"

#include "biosim/core/challenges.h"
#include "biosim/core/grid.h"
#include "biosim/core/io_catalogue.h"
#include "biosim/core/nnet.h"
#include "biosim/core/rng.h"
#include "biosim/core/types.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── survivor collection ────────────────────────────────────────────────────*/

static int cmp_u64(const void *a, const void *b) {
    uint64_t x = *(const uint64_t *)a;
    uint64_t y = *(const uint64_t *)b;
    return (x > y) - (x < y);
}

/*
 * Evaluate the challenge for every alive agent and collect passing indices into
 * survivors[].  Computes and fills all biosim_gen_stats_t fields.
 * survivors must point to a caller-allocated array with at least pop elements.
 * Returns the number of survivors found.
 */
static uint32_t collect_survivors(biosim_stepper_t *stepper, uint32_t *survivors,
                                  biosim_gen_stats_t *stats) {
    biosim_context_t *ctx = &stepper->base;
    const uint32_t pop = ctx->agents.capacity;

    uint32_t n = 0;
    double score_sum = 0.0;
    double len_sum = 0.0;
    double len_sq = 0.0;

    for (uint32_t i = 0; i < pop; i++) {
        if (!ctx->agents.alive[i]) {
            continue;
        }
        biosim_challenge_result_t r = biosim_challenge_eval(&ctx->challenge, i, ctx);
        if (!r.passed) {
            continue;
        }
        survivors[n++] = i;
        score_sum += (double)r.score;
        double glen = (double)ctx->genome.length[i];
        len_sum += glen;
        len_sq += glen * glen;
    }

    stats->gen = stepper->gen;
    stats->population = pop;
    stats->survivors = n;

    if (n == 0) {
        stats->survival_rate = 0.0F;
        stats->genome_len_mean = 0.0F;
        stats->genome_len_std = 0.0F;
        stats->unique_phenotypes = 0;
        stats->phenotype_div = 0.0F;
        stats->score_mean = 0.0F;
        return 0;
    }

    double dn = (double)n;
    stats->survival_rate = (float)n / (float)pop;
    stats->genome_len_mean = (float)(len_sum / dn);
    stats->score_mean = (float)(score_sum / dn);

    double mean = len_sum / dn;
    double var = (len_sq / dn) - (mean * mean);
    stats->genome_len_std = (var > 0.0) ? (float)sqrt(var) : 0.0F;

    /* phenotype diversity: sort fingerprints of survivors, count distinct */
    uint64_t *fps = malloc(n * sizeof(uint64_t));
    if (fps != NULL) {
        for (uint32_t j = 0; j < n; j++) {
            fps[j] = ctx->agents.genome_fingerprint[survivors[j]];
        }
        qsort(fps, (size_t)n, sizeof(uint64_t), cmp_u64);
        uint32_t unique = 1;
        for (uint32_t j = 1; j < n; j++) {
            if (fps[j] != fps[j - 1]) {
                unique++;
            }
        }
        free(fps);
        stats->unique_phenotypes = unique;
        stats->phenotype_div = (float)unique / (float)n;
    } else {
        stats->unique_phenotypes = 0;
        stats->phenotype_div = 0.0F;
    }

    return n;
}

/* ── reproduction ───────────────────────────────────────────────────────────*/

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
    const uint32_t cap = genome->capacity;
    const uint16_t max_len = genome->max_length;

    for (uint32_t s = 0; s < n_survivors; s++) {
        const uint32_t src = survivors[s];
        const uint16_t len = genome->length[src];
        temp_len[s] = len;
        for (uint16_t j = 0; j < len; j++) {
            temp_conn[(size_t)s * max_len + j] = genome->conn[(size_t)j * cap + src];
            temp_wgt[(size_t)s * max_len + j] = genome->wgt[(size_t)j * cap + src];
        }
    }
}

/* Write snapshot entry parent_s into genome slot dst. */
static void restore_genome_slot(biosim_genome_t *genome, uint32_t dst, const uint16_t *temp_conn,
                                const int16_t *temp_wgt, const uint16_t *temp_len,
                                uint32_t parent_s) {
    const uint32_t cap = genome->capacity;
    const uint16_t max_len = genome->max_length;
    const uint16_t len = temp_len[parent_s];

    genome->length[dst] = len;
    for (uint16_t j = 0; j < len; j++) {
        genome->conn[(size_t)j * cap + dst] = temp_conn[(size_t)parent_s * max_len + j];
        genome->wgt[(size_t)j * cap + dst] = temp_wgt[(size_t)parent_s * max_len + j];
    }
}

static void reproduce(biosim_stepper_t *stepper, const uint32_t *survivors, uint32_t n_survivors) {
    biosim_context_t *ctx = &stepper->base;
    biosim_genome_t *genome = &ctx->genome;
    biosim_nnet_t *nnet = &ctx->nnet;
    biosim_agents_t *agents = &ctx->agents;
    biosim_grid_t *grid = &ctx->grid;

    const uint32_t pop = agents->capacity;
    const uint16_t max_len = genome->max_length;
    const uint8_t long_probe_dist = agents->long_probe_dist[0];

    /* clear non-barrier grid cells */
    for (int y = 0; y < (int)grid->size_y; y++) {
        for (int x = 0; x < (int)grid->size_x; x++) {
            uint16_t *cell = &grid->cells[y * grid->size_x + x];
            if (*cell != BIOSIM_GRID_BARRIER) {
                *cell = BIOSIM_GRID_EMPTY;
            }
        }
    }

    /* clear signal layer */
    memset(ctx->signal, 0, ctx->signal_len * sizeof(uint32_t));

    /* snapshot survivor genomes before any slot is overwritten */
    uint16_t *temp_conn = NULL;
    int16_t *temp_wgt = NULL;
    uint16_t *temp_len = NULL;

    if (n_survivors > 0) {
        temp_conn = malloc((size_t)n_survivors * max_len * sizeof(uint16_t));
        temp_wgt = malloc((size_t)n_survivors * max_len * sizeof(int16_t));
        temp_len = malloc((size_t)n_survivors * sizeof(uint16_t));
        if (temp_conn != NULL && temp_wgt != NULL && temp_len != NULL) {
            snapshot_survivor_genomes(genome, survivors, n_survivors, temp_conn, temp_wgt,
                                      temp_len);
        } else {
            /* allocation failure: fall back to extinction path */
            free(temp_conn);
            free(temp_wgt);
            free(temp_len);
            temp_conn = NULL;
            temp_wgt = NULL;
            temp_len = NULL;
        }
    }

    const uint64_t gen_seed = biosim_rng_next(&stepper->gen_rng);

    for (uint32_t i = 0; i < pop; i++) {
        if (n_survivors == 0 || temp_conn == NULL) {
            uint16_t rand_len =
                (uint16_t)(1U + (uint16_t)(biosim_rng_next(&stepper->gen_rng) % (uint64_t)max_len));
            biosim_genome_init_slot(genome, i, rand_len, &stepper->gen_rng);
        } else {
            uint32_t parent_s =
                (uint32_t)(biosim_rng_next(&stepper->gen_rng) % (uint64_t)n_survivors);
            restore_genome_slot(genome, i, temp_conn, temp_wgt, temp_len, parent_s);
            biosim_genome_mutate(genome, i, stepper->mutation_rate, &stepper->gen_rng);
        }

        biosim_nnet_compile_slot(nnet, genome, i, BIOSIM_NUM_SENSORS, BIOSIM_NUM_ACTIONS);
        const uint64_t fp = biosim_nnet_fingerprint(nnet, i);

        biosim_coord_t loc;
        (void)biosim_grid_find_empty(grid, &stepper->gen_rng, &loc);
        biosim_agents_init_slot(agents, i, loc, long_probe_dist, biosim_rng_seed(i, gen_seed));
        agents->genome_fingerprint[i] = fp;
        biosim_grid_set(grid, loc, (uint16_t)(i + 1U));
    }

    free(temp_conn);
    free(temp_wgt);
    free(temp_len);
}

/* ── public API ─────────────────────────────────────────────────────────────*/

biosim_gen_stats_t biosim_stepper_advance_gen(biosim_stepper_t *stepper) {
    const uint32_t pop = stepper->base.agents.capacity;

    biosim_gen_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    stats.gen = stepper->gen;
    stats.population = pop;

    uint32_t *survivors = malloc(pop * sizeof(uint32_t));
    uint32_t n_survivors = 0;
    if (survivors != NULL) {
        n_survivors = collect_survivors(stepper, survivors, &stats);
    }

    reproduce(stepper, survivors, n_survivors);
    free(survivors);

    stepper->step = 0;
    stepper->gen++;

    return stats;
}

/* ── print ──────────────────────────────────────────────────────────────────*/

void biosim_gen_stats_print_header(void) {
    (void)printf("%5s %7s %7s %8s %8s %7s %9s\n", "gen", "alive", "surv%", "glen.m", "glen.s",
                 "pdiv%", "score.m");
}

void biosim_gen_stats_print(const biosim_gen_stats_t *stats) {
    (void)printf("%5u %7u %6.1f%% %8.2f %8.2f %6.1f%% %9.4f\n", stats->gen, stats->survivors,
                 (double)(stats->survival_rate * 100.0F), (double)stats->genome_len_mean,
                 (double)stats->genome_len_std, (double)(stats->phenotype_div * 100.0F),
                 (double)stats->score_mean);
}
