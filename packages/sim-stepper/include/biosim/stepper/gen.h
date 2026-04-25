/*
 * HOST-ONLY: references biosim_stepper_t which embeds heap-pointer structs.
 * Do NOT include from OpenCL kernel sources (.cl files).
 */
#ifndef BIOSIM_STEPPER_GEN_H
#define BIOSIM_STEPPER_GEN_H

#include "biosim/stepper/step.h"
#include <stdint.h>

/*
 * Per-generation statistics collected from the population at generation end,
 * before reproduction.  All fields refer to agents that passed the challenge
 * (survivors), except population which is the full agent count.
 */
typedef struct {
    uint32_t gen;
    uint32_t population;
    uint32_t survivors;         /* agents that passed the challenge */
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
 * After the call: step is reset to 0 and gen is incremented.
 * Returns statistics computed from the just-completed generation.
 */
biosim_gen_stats_t biosim_stepper_advance_gen(biosim_stepper_t *stepper);

/* Print the aligned column header — call once before the generation loop. */
void biosim_gen_stats_print_header(void);

/* Print one generation's statistics aligned to the header columns. */
void biosim_gen_stats_print(const biosim_gen_stats_t *stats);

#endif /* BIOSIM_STEPPER_GEN_H */
