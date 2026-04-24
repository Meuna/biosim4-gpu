/*
 * HOST-ONLY: embeds agents/grid/genome/nnet which carry heap pointers.
 * Do NOT include from OpenCL kernel sources (.cl files).
 */
#ifndef BIOSIM_CORE_CONTEXT_H
#define BIOSIM_CORE_CONTEXT_H

#include "biosim/core/agents.h"
#include "biosim/core/barriers.h"
#include "biosim/core/genome.h"
#include "biosim/core/grid.h"
#include "biosim/core/nnet.h"
#include "biosim/core/status.h"
#include <stddef.h>
#include <stdint.h>

/*
 * Full simulation state passed to core algorithms by the simulator.
 * Configuration fields are populated from biosim_params_t by the simulator's
 * own create function; resource fields are allocated individually using the
 * biosim_agents_create / biosim_grid_create / etc. helpers.
 */
typedef struct {
    /* configuration */
    int steps_per_gen;
    int population_sensor_radius;

    /* simulation resources */
    biosim_agents_t agents;
    biosim_grid_t grid;
    biosim_genome_t genome;
    biosim_nnet_t nnet;
    uint32_t *signal;  /* flat [size_y * size_x], row-major */
    size_t signal_len; /* cached size_x * size_y */
} biosim_context_t;

/* Lifecycle */
biosim_status_t biosim_context_create(uint32_t pop, int16_t size_x, int16_t size_y,
                                      int steps_per_gen, uint16_t max_gen_len, uint8_t max_neurons,
                                      uint8_t long_probe_dist, int pop_sensor_radius,
                                      const biosim_barrier_spec_t *barriers, int n_barriers,
                                      biosim_context_t *out);

void biosim_context_free(biosim_context_t *ctx);

#endif /* BIOSIM_CORE_CONTEXT_H */
