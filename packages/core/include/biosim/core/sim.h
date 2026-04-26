/*
 * HOST-ONLY: embeds agents/grid/genome/nnet which carry heap pointers.
 * Do NOT include from OpenCL kernel sources (.cl files).
 */
#ifndef BIOSIM_CORE_SIM_H
#define BIOSIM_CORE_SIM_H

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
 * Per-generation statistics collected from the population at generation end,
 * before reproduction.
 *
 * survivors    — agents that passed the challenge (will reproduce)
 * kills        — agents killed by KILL_FORWARD during the generation
 *                (0 when enable_kill is false)
 *
 * survival_rate = survivors / population
 */
typedef struct {
    uint32_t gen;
    uint32_t population;
    uint32_t survivors;         /* agents that passed the challenge */
    uint32_t kills;             /* agents killed by KILL_FORWARD */
    float survival_rate;        /* survivors / population */
    float genome_len_mean;      /* mean genome length of survivors */
    float genome_len_std;       /* std dev of genome lengths — variability */
    uint32_t unique_phenotypes; /* distinct compiled-nnet fingerprints among survivors */
    float phenotype_div;        /* unique_phenotypes / survivors */
    float score_mean;           /* mean challenge score of survivors */
} biosim_gen_stats_t;

/*
 * Full simulation state for the single-threaded reference implementation.
 *
 * Populate the configuration fields, then call biosim_sim_create() to
 * allocate all heap resources.  biosim_sim_free() releases them.
 */
typedef struct {
    /* ── allocation-time configuration ───────────────────────────────────────
     * Set these before calling biosim_sim_create(); they are read once
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
     * Managed by biosim_challenge_step() and biosim_sim_advance_gen(). */
    uint32_t step;       /* step index within the current generation */
    uint32_t gen;        /* generation index (0-based) */
    float mutation_rate; /* per-gene point-mutation probability */
    uint64_t gen_rng;    /* RNG state for generation-boundary operations */

    /* ── simulation resources ────────────────────────────────────────────────
     * Allocated by biosim_sim_create(); released by biosim_sim_free().*/
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
} biosim_sim_t;

/*
 * Lifecycle
 *
 * biosim_sim_create — allocate all heap resources described by the
 * configuration fields already set in *sim, place barriers on the grid,
 * and spawn the initial population.  barriers/n_barriers may be NULL/0.
 */
biosim_status_t biosim_sim_create(biosim_sim_t *sim, const biosim_barrier_spec_t *barriers,
                                  int n_barriers);

void biosim_sim_free(biosim_sim_t *sim);

/* Advance one agent by one step: evaluate sensors, run feedforward, apply
 * actions, finalize movement. */
void biosim_sim_step_agent(biosim_sim_t *sim, uint32_t i);

/* Finalize the current step: fade the signal layer, run the per-step
 * challenge hook, and increment sim->step. */
void biosim_sim_next_step(biosim_sim_t *sim);

/*
 * Advance one generation: evaluate the challenge for all alive agents, collect
 * statistics, reproduce survivors (asexual: copy + mutate), recompile neural
 * networks, and respawn the full population on the grid.
 *
 * After the call: sim->step is reset to 0 and sim->gen is incremented.
 * stats receives the metrics computed from the just-completed generation.
 */
void biosim_sim_next_generation(biosim_sim_t *sim, biosim_gen_stats_t *stats);

#endif /* BIOSIM_CORE_SIM_H */
