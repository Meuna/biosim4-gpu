/*
 * HOST-ONLY: this header uses heap pointers and host standard types.
 * Do NOT include it from OpenCL kernel sources (.cl files).
 * GPU kernels receive individual int32_t * / float * buffer arguments.
 */
#ifndef BIOSIM_CORE_AGENTS_H
#define BIOSIM_CORE_AGENTS_H

#include "biosim/core/grid_defs.h"
#include "biosim/core/status.h"
#include <stdint.h>

/*
 * Per-agent data in Structure of Arrays layout.
 * Slots are indexed 0..population-1. All buffers are always allocated;
 * alive[i] == 0 marks a slot as inactive.
 */
typedef struct {
    uint32_t population;

    /* Position (split SoA for independent coalesced access on GPU) */
    int32_t *loc_x;
    int32_t *loc_y;
    int32_t *birth_x;
    int32_t *birth_y;

    /* Per-agent state */
    uint8_t *alive;
    uint16_t *osc_period;
    float *responsiveness;
    uint8_t *long_probe_dist;
    uint8_t *last_move_dir;
    uint8_t *kill_marker;
    uint32_t *challenge_bits;
    uint64_t *rng_state;
    uint64_t *genome_fingerprint; /* zeroed until genome module is implemented */

    /* Transient per-step movement targets (feedforward → movement kernel) */
    int32_t *desired_x;
    int32_t *desired_y;
    float *dx_sum; /* accumulated action movement in x before finalisation */
    float *dy_sum; /* accumulated action movement in y before finalisation */
} biosim_agents_t;

/* Lifecycle */
biosim_status_t biosim_agents_create(uint32_t population, biosim_agents_t *out);
void biosim_agents_free(biosim_agents_t *agents);

/*
 * Initialise one slot ready for simulation: marks it alive, sets position,
 * applies biological defaults, and seeds its RNG from (idx, rng_seed).
 */
void biosim_agents_init_slot(biosim_agents_t *agents, uint32_t idx, biosim_coord_t loc,
                             uint8_t long_probe_dist, uint64_t rng_seed);

#endif /* BIOSIM_CORE_AGENTS_H */
