/*
 * HOST-ONLY: references biosim_sim_t which carries heap pointers.
 * Do NOT include from OpenCL kernel sources (.cl files).
 */
#ifndef BIOSIM_CORE_GENERATION_H
#define BIOSIM_CORE_GENERATION_H

#include "biosim/core/sim.h"
#include <stdint.h>

/*
 * Evaluate the challenge for every alive agent and collect passing indices into
 * survivors[], with the corresponding fitness score (0..1) into scores[].
 * Both arrays must each point to a caller-allocated buffer with at least
 * sim->agents.population elements. Returns the number of survivors found.
 */
uint32_t biosim_generation_collect_survivors(biosim_sim_t *sim, uint32_t *survivors, float *scores);

/*
 * Initialise the full population from scratch: clear non-barrier grid cells
 * and the signal layer, generate random genomes, recompile neural networks,
 * and place all agents. Uses and advances sim->gen_rng.
 * Barriers must already be placed on the grid before calling this.
 */
biosim_status_t biosim_generation_init_random(biosim_sim_t *sim);

/*
 * Reproduce: clear the grid, snapshot survivor genomes, repopulate every slot
 * with genomes derived from survivor parents (with mutation), recompile neural
 * networks, and place agents on the grid.
 *
 * scores[] is the parallel fitness array from biosim_generation_collect_survivors.
 * It is used when sim->choose_parents_by_fitness is true (score-biased selection).
 * sim->sexual_reproduction controls whether crossover is applied.
 *
 * Precondition: n_survivors > 0.
 * Returns BIOSIM_ERR_NOMEM if any required allocation fails.
 */
biosim_status_t biosim_generation_reproduce(biosim_sim_t *sim, uint32_t *survivors, float *scores,
                                            uint32_t n_survivors);

#endif /* BIOSIM_CORE_GENERATION_H */
