/*
 * HOST-ONLY: embeds agents/grid/genome/nnet which carry heap pointers.
 * Do NOT include from OpenCL kernel sources (.cl files).
 */
#ifndef BIOSIM_CORE_CONTEXT_H
#define BIOSIM_CORE_CONTEXT_H

#include "biosim/core/agents.h"
#include "biosim/core/barriers.h"
#include "biosim/core/challenge_spec.h"
#include "biosim/core/genome.h"
#include "biosim/core/grid.h"
#include "biosim/core/nnet.h"
#include "biosim/core/status.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Full simulation state for the single-threaded reference implementation.
 *
 * Populate the configuration fields, then call biosim_context_create() to
 * allocate all heap resources.  biosim_context_free() releases them.
 */
typedef struct {
    /* ── allocation-time configuration ───────────────────────────────────────
     * Set these before calling biosim_context_create(); they are read once
     * during allocation and remain valid for the lifetime of the context.*/
    uint32_t population;     /* agent count */
    int16_t size_x;          /* grid width */
    int16_t size_y;          /* grid height */
    uint16_t max_gen_len;    /* maximum genome length (genes per agent) */
    uint8_t max_neurons;     /* maximum hidden-neuron count per agent */
    uint8_t long_probe_dist; /* default long-probe sensor range (cells) */

    /* ── runtime configuration ───────────────────────────────────────────────
     * Set these before or after create(); they are read every step/gen.     */
    int steps_per_gen;
    int population_sensor_radius;
    bool enable_kill;
    biosim_challenge_spec_t challenge;

    /* ── generation state ────────────────────────────────────────────────────
     * Managed by biosim_challenge_step() and biosim_context_advance_gen(). */
    uint32_t step;       /* step index within the current generation */
    uint32_t gen;        /* generation index (0-based) */
    float mutation_rate; /* per-gene point-mutation probability */
    uint64_t gen_rng;    /* RNG state for generation-boundary operations */

    /* ── simulation resources ────────────────────────────────────────────────
     * Allocated by biosim_context_create(); released by biosim_context_free().*/
    biosim_agents_t agents;
    biosim_grid_t grid;
    biosim_genome_t genome;
    biosim_nnet_t nnet;
    uint32_t *signal;  /* flat [size_y * size_x], row-major */
    size_t signal_len; /* cached size_x * size_y */

    /* barrier centres resolved at creation time; used by near_barrier and
     * location_sequence challenge kinds; NULL when n_barrier_ctrs == 0 */
    biosim_coord_t *barrier_ctrs;
    int n_barrier_ctrs;

    /* kill tracking — reset to 0 at each generation boundary */
    uint32_t kills;
} biosim_context_t;

/*
 * Lifecycle
 *
 * biosim_context_create — allocate all heap resources described by the
 * configuration fields already set in *ctx, place barriers on the grid,
 * and spawn the initial population.  barriers/n_barriers may be NULL/0.
 */
biosim_status_t biosim_context_create(biosim_context_t *ctx, const biosim_barrier_spec_t *barriers,
                                      int n_barriers);

void biosim_context_free(biosim_context_t *ctx);

#endif /* BIOSIM_CORE_CONTEXT_H */
