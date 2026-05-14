/*
 * HOST-ONLY: this header references biosim_sim_t which contains heap
 * pointers. Do NOT include from OpenCL kernel sources (.cl files).
 */
#ifndef BIOSIM_CORE_IO_CATALOGUE_H
#define BIOSIM_CORE_IO_CATALOGUE_H

#include "biosim/core/io_defs.h"
#include "biosim/core/sim.h"
#include <stdint.h>

/* ── public API ─────────────────────────────────────────────────────────── */

/* Compute the direction of the movement, according to the convention used by
 * the agent IO */
uint8_t biosim_get_dir(int dx, int dy);

/* Evaluate one sensor for agent idx; returns float in [0,1].
 * agents.rng_state[idx] may be mutated by BIOSIM_SENSOR_RANDOM.
 * Asserts on an invalid sensor value (out-of-range enum). */
float biosim_sensor_eval(biosim_sensor_t sensor, uint32_t idx, const biosim_sim_t *sim);

/* Apply one action to agent idx, writing into sim->agents.dx_sum[idx] / dy_sum[idx]
 * and agent self-fields.
 * Caller must zero dx_sum[idx]/dy_sum[idx] before the first call per agent.
 * Asserts on an invalid action value (out-of-range enum). */
void biosim_action_apply(biosim_action_t action, float val, uint32_t idx, biosim_sim_t *sim);

/* Convert sim->agents.dx_sum[idx]/dy_sum[idx] to desired_x/desired_y using a
 * probabilistic step. Call once per agent after all actions have fired.
 * The result is a proposal; grid availability is resolved by
 * biosim_sim_next_step. */
void biosim_action_propose_move(uint32_t idx, biosim_sim_t *sim);

#endif /* BIOSIM_CORE_IO_CATALOGUE_H */
