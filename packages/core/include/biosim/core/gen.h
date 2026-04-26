/*
 * HOST-ONLY: references biosim_sim_t which carries heap pointers.
 * Do NOT include from OpenCL kernel sources (.cl files).
 */
#ifndef BIOSIM_CORE_GEN_H
#define BIOSIM_CORE_GEN_H

#include "biosim/core/sim.h"
#include <stdint.h>

/*
 * Per-generation statistics collected from the population at generation end,
 * before reproduction.
 *
 * survivors    — agents that passed the challenge (will reproduce)
 * kills        — agents killed by KILL_FORWARD during the generation
 *                (0 when enable_kill is false)
 *
 * survival_rate = survivors / population
 */
typedef struct {
    uint32_t gen;
    uint32_t population;
    uint32_t survivors;         /* agents that passed the challenge */
    uint32_t kills;             /* agents killed by KILL_FORWARD */
    float survival_rate;        /* survivors / population */
    float genome_len_mean;      /* mean genome length of survivors */
    float genome_len_std;       /* std dev of genome lengths — variability */
    uint32_t unique_phenotypes; /* distinct compiled-nnet fingerprints among survivors */
    float phenotype_div;        /* unique_phenotypes / survivors */
    float score_mean;           /* mean challenge score of survivors */
} biosim_gen_stats_t;

/*
 * Advance one generation: evaluate the challenge for all alive agents, collect
 * statistics, reproduce survivors (asexual: copy + mutate), recompile neural
 * networks, and respawn the full population on the grid.
 *
 * After the call: sim->step is reset to 0 and sim->gen is incremented.
 * stats receives the metrics computed from the just-completed generation.
 */
void biosim_sim_advance_gen(biosim_sim_t *sim, biosim_gen_stats_t *stats);

#endif /* BIOSIM_CORE_GEN_H */
