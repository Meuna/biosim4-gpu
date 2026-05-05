/*
 * HOST-ONLY: this header references biosim_sim_t which contains heap
 * pointers. Do NOT include from OpenCL kernel sources (.cl files).
 */
#ifndef BIOSIM_CORE_IO_CATALOGUE_H
#define BIOSIM_CORE_IO_CATALOGUE_H

#include "biosim/core/sim.h"
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
    /* Sentinel */
    BIOSIM_NUM_SENSORS
} biosim_sensor_t;

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
    /* Sentinel */
    BIOSIM_NUM_ACTIONS
} biosim_action_t;

/* ── schema version ─────────────────────────────────────────────────────── */

/* Bump when sensor/action indices or gene-encoding semantics change.
 * Snapshot files store this value; a mismatch means genomes are incompatible. */
#define BIOSIM_IO_SCHEMA_VERSION 1U

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
 * probabilistic step. Call once per agent after all actions have fired. */
void biosim_action_finalize_movement(uint32_t idx, biosim_sim_t *sim);

#endif /* BIOSIM_CORE_IO_CATALOGUE_H */
