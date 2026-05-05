/*
 * HOST-ONLY: includes sim.h which carries heap pointers.
 * Do NOT include from OpenCL kernel sources (.cl files).
 */
#ifndef BIOSIM_CORE_CHALLENGES_H
#define BIOSIM_CORE_CHALLENGES_H

#include "biosim/core/challenge_spec.h"
#include "biosim/core/sim.h"

/*
 * Evaluate whether the agent at agent_idx passes the challenge at generation
 * end. The full sim state provides location, birth location, challenge_bits,
 * grid occupancy, and barrier centres.
 */
biosim_challenge_result_t biosim_challenge_eval(const biosim_challenge_spec_t *spec,
                                                uint32_t agent_idx, const biosim_sim_t *sim);

/*
 * Per-simulation-step bookkeeping hook. Must be called once after all agents
 * have moved each step. Updates challenge_bits and alive flags in-place for
 * kinds that track state across steps (touch_any_wall, radioactive_walls,
 * location_sequence). No-op for all other kinds.
 */
void biosim_challenge_step(biosim_sim_t *sim);

#endif /* BIOSIM_CORE_CHALLENGES_H */
