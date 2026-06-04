/*
 * HOST-ONLY: references biosim_sim_t which carries heap pointers.
 * Do NOT include from OpenCL kernel sources (.cl files).
 */
#ifndef BIOSIM_CORE_GENERATION_H
#define BIOSIM_CORE_GENERATION_H

#include "biosim/core/sim.h"
#include "biosim/core/survivor_snap.h"

/*
 * Evaluate the challenge for every alive agent, grow snap as needed, and fill
 * snap->conn/wgt/len/scores with compact row-major survivor genomes.
 * Sets snap->count to the number of survivors found.
 * Returns BIOSIM_ERR_NOMEM if snap growth or a temporary allocation fails.
 */
biosim_status_t biosim_generation_collect_survivors(
    biosim_sim_t *sim, biosim_survivor_snap_t *snap
);

/*
 * Initialise the full population from scratch: clear non-barrier grid cells
 * and the signal layer, generate random genomes, recompile neural networks,
 * and place all agents. Uses and advances sim->gen_rng.
 * Barriers must already be placed on the grid before calling this.
 */
biosim_status_t biosim_generation_init_random(biosim_sim_t *sim);

/*
 * Breed the next population from a compact genome snapshot.
 * Clears the grid, then repopulates every slot with genomes derived from
 * snap->conn/wgt/len/scores (with mutation and optional crossover/fitness-bias).
 * sim->sexual_reproduction and sim->choose_parents_by_fitness control behaviour.
 * Precondition: snap->count > 0.
 * Returns BIOSIM_ERR_NOMEM if any required allocation fails.
 */
biosim_status_t biosim_generation_breed(biosim_sim_t *sim, const biosim_survivor_snap_t *snap);

/*
 * Spawn the next generation: calls breed if snap->count > 0, else init_random.
 * This is the single entry point at the start of every generation after the first.
 * Returns BIOSIM_ERR_NOMEM on allocation failure.
 */
biosim_status_t biosim_generation_spawn(biosim_sim_t *sim, biosim_survivor_snap_t *snap);

#endif /* BIOSIM_CORE_GENERATION_H */
