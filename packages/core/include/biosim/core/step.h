/*
 * HOST-ONLY: references biosim_context_t which carries heap pointers.
 * Do NOT include from OpenCL kernel sources (.cl files).
 */
#ifndef BIOSIM_CORE_STEP_H
#define BIOSIM_CORE_STEP_H

#include "biosim/core/context.h"

/*
 * Advance the simulation by one step for one agent.
 *
 * For one agent: evaluate sensors, run feedforward, apply actions,
 * finalise movement.  Then decay the signal layer and invoke the
 * per-step challenge hook.  Increments ctx->step.
 */
void step_agent(biosim_context_t *ctx, uint32_t i);

#endif /* BIOSIM_CORE_STEP_H */
