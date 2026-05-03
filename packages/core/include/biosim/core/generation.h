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
 * survivors[]. survivors must point to a caller-allocated array with at least
 * pop elements. Returns the number of survivors found.
 */
uint32_t biosim_generation_collect_survivors(biosim_sim_t *sim, uint32_t *survivors);

/*
 * Initialise the full population from scratch: clear non-barrier grid cells
 * and the signal layer, generate random genomes, recompile neural networks,
 * and place all agents. Uses and advances sim->gen_rng.
 * Barriers must already be placed on the grid before calling this.
 */
biosim_status_t biosim_generation_init_random(biosim_sim_t *sim);

/*
 * Reproduce: clear the grid, snapshot survivor genomes, repopulate every slot
 * by copying a random survivor genome (with mutation), recompile neural
 * networks, and place agents on the grid.
 *
 * Precondition: n_survivors > 0.
 * If the internal genome snapshot allocation fails, falls back to
 * biosim_generation_init_random automatically.
 */
biosim_status_t biosim_generation_reproduce(biosim_sim_t *sim, const uint32_t *survivors,
                                            uint32_t n_survivors);

#endif /* BIOSIM_CORE_GENERATION_H */
