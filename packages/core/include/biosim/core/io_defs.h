/*
 * HOST/DEVICE PORTABILITY: this header is included by OpenCL kernel sources.
 * Do NOT add <stdio.h>, <stdlib.h>, <string.h>, or any other host-only header.
 * Keep to enum definitions and simple constant data that compile in both C11
 * and OpenCL C.
 */
#ifndef BIOSIM_CORE_IO_DEFS_H
#define BIOSIM_CORE_IO_DEFS_H

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

/* ── direction tables (0=E, CCW: E NE N NW W SW S SE) ─────────────────── */

#ifdef __OPENCL_VERSION__
__constant int BIOSIM_DIR_DX[8] = {1, 1, 0, -1, -1, -1, 0, 1};
__constant int BIOSIM_DIR_DY[8] = {0, -1, -1, -1, 0, 1, 1, 1};
#else
static const int BIOSIM_DIR_DX[8] = {1, 1, 0, -1, -1, -1, 0, 1};
static const int BIOSIM_DIR_DY[8] = {0, -1, -1, -1, 0, 1, 1, 1};
#endif

#endif /* BIOSIM_CORE_IO_DEFS_H */
