/*
 * HOST-ONLY: this header references biosim_agents_t and biosim_grid_t which
 * contain heap pointers. Do NOT include from OpenCL kernel sources (.cl files).
 */
#ifndef BIOSIM_CORE_IO_CATALOGUE_H
#define BIOSIM_CORE_IO_CATALOGUE_H

#include "biosim/core/agents.h"
#include "biosim/core/grid.h"
#include "biosim/core/params.h"
#include <stdint.h>

/* ── sensor catalogue ───────────────────────────────────────────────────── */

typedef enum {
    /* Group A — self-only (per-agent fields, no grid/signal access) */
    BIOSIM_SENSOR_LOC_X = 0,
    BIOSIM_SENSOR_LOC_Y = 1,
    BIOSIM_SENSOR_BOUNDARY_DIST_X = 2,
    BIOSIM_SENSOR_BOUNDARY_DIST_Y = 3,
    BIOSIM_SENSOR_BOUNDARY_DIST = 4,
    BIOSIM_SENSOR_LAST_MOVE_DIR_X = 5,
    BIOSIM_SENSOR_LAST_MOVE_DIR_Y = 6,
    BIOSIM_SENSOR_OSC1 = 7,
    BIOSIM_SENSOR_AGE = 8,
    BIOSIM_SENSOR_RANDOM = 9,
    /* Group B — grid neighbourhood (placeholder sensors return 0.5) */
    BIOSIM_SENSOR_POPULATION = 10,
    BIOSIM_SENSOR_POPULATION_FWD = 11,    /* placeholder */
    BIOSIM_SENSOR_POPULATION_LR = 12,     /* placeholder */
    BIOSIM_SENSOR_BARRIER_FWD = 13,       /* placeholder */
    BIOSIM_SENSOR_BARRIER_LR = 14,        /* placeholder */
    BIOSIM_SENSOR_LONGPROBE_POP_FWD = 15, /* placeholder */
    BIOSIM_SENSOR_LONGPROBE_BAR_FWD = 16, /* placeholder */
    /* Group C — signal layer (directional variants are placeholders) */
    BIOSIM_SENSOR_SIGNAL0 = 17,
    BIOSIM_SENSOR_SIGNAL0_FWD = 18, /* placeholder */
    BIOSIM_SENSOR_SIGNAL0_LR = 19,  /* placeholder */
    /* Group D — cross-agent */
    BIOSIM_SENSOR_GENETIC_SIM_FWD = 20,
} biosim_sensor_t;

#define BIOSIM_NUM_SENSORS 21U

/* ── action catalogue ───────────────────────────────────────────────────── */

typedef enum {
    /* Group A — self-field writers */
    BIOSIM_ACTION_SET_RESPONSIVENESS = 0,
    BIOSIM_ACTION_SET_OSCILLATOR_PERIOD = 1,
    BIOSIM_ACTION_SET_LONGPROBE_DIST = 2,
    /* Group B — movement accumulators */
    BIOSIM_ACTION_MOVE_X = 3,
    BIOSIM_ACTION_MOVE_Y = 4,
    BIOSIM_ACTION_MOVE_FORWARD = 5,
    BIOSIM_ACTION_MOVE_REVERSE = 6,
    BIOSIM_ACTION_MOVE_LEFT = 7,
    BIOSIM_ACTION_MOVE_RIGHT = 8,
    BIOSIM_ACTION_MOVE_RL = 9,
    BIOSIM_ACTION_MOVE_RANDOM = 10,
    BIOSIM_ACTION_MOVE_EAST = 11,
    BIOSIM_ACTION_MOVE_WEST = 12,
    BIOSIM_ACTION_MOVE_NORTH = 13,
    BIOSIM_ACTION_MOVE_SOUTH = 14,
    /* Group C — signal emission */
    BIOSIM_ACTION_EMIT_SIGNAL0 = 15,
    /* Group D — kill */
    BIOSIM_ACTION_KILL_FORWARD = 16,
} biosim_action_t;

#define BIOSIM_NUM_ACTIONS 17U

/* ── sensor evaluation context ──────────────────────────────────────────── */

/*
 * All fields the sensor functions may read.  agents->rng_state[idx] is
 * mutated by BIOSIM_SENSOR_RANDOM; this is valid C because const does not
 * propagate through pointer members of a const-qualified struct pointer.
 */
typedef struct {
    uint32_t idx;                  /* agent being evaluated */
    const biosim_agents_t *agents; /* per-agent SoA (rng_state writable) */
    const biosim_grid_t *grid;
    const uint32_t *signal; /* signal layer 0, size_x*size_y, row-major */
    uint32_t sim_step;
} biosim_sense_ctx_t;

/* ── action application context ─────────────────────────────────────────── */

/*
 * dx_sum / dy_sum are caller-zeroed accumulators: zero them before the first
 * biosim_action_apply call for an agent, then call biosim_action_finalize_movement
 * once after all actions have been applied.
 */
typedef struct {
    uint32_t idx;
    biosim_agents_t *agents;
    const biosim_grid_t *grid; /* read-only: forward-cell lookup for KILL_FORWARD */
    uint32_t *signal;          /* writable: EMIT_SIGNAL0 */
    float dx_sum;
    float dy_sum;
} biosim_act_ctx_t;

/* ── public API ─────────────────────────────────────────────────────────── */

/* Evaluate one sensor; returns float in [0.0, 1.0].
 * Asserts on an invalid sensor value (out-of-range enum). */
float biosim_sensor_eval(biosim_sensor_t sensor, const biosim_sense_ctx_t *ctx,
                         const biosim_params_t *params);

/* Apply one action to ctx, updating dx_sum/dy_sum and agent self-fields.
 * Asserts on an invalid action value (out-of-range enum). */
void biosim_action_apply(biosim_action_t action, float val, biosim_act_ctx_t *ctx,
                         const biosim_params_t *params);

/* Convert accumulated dx_sum/dy_sum to desired_x/desired_y using a
 * probabilistic step.  Call once per agent after all actions have fired. */
void biosim_action_finalize_movement(biosim_act_ctx_t *ctx, const biosim_params_t *params);

#endif /* BIOSIM_CORE_IO_CATALOGUE_H */
