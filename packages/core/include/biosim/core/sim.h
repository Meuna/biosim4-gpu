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
#include "biosim/core/params.h"
#include "biosim/core/status.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

struct biosim_census;

/*
 * Full simulation state for the single-threaded reference implementation.
 *
 * Call biosim_sim_create() with a resolved biosim_params_t to configure all
 * fields, allocate all heap resources, and spawn the initial population.
 * biosim_sim_free() releases them.
 */
typedef struct {
    uint32_t max_generations; /* number of generation loops */

    /* ── allocation-time configuration ─────────────────────────────────────── */
    uint32_t population;     /* agent count */
    int32_t size_x;          /* grid width */
    int32_t size_y;          /* grid height */
    uint16_t genome_max_len; /* maximum genome length (genes per agent) */
    uint8_t max_neurons;     /* maximum hidden-neuron count per agent */
    uint8_t los_range;       /* default long-probe sensor range (cells) */

    /* ── runtime configuration ──────────────────────────────────────────────── */
    uint32_t steps_per_gen;
    int32_t sensor_radius;
    bool enable_kill;
    float responsiveness_curve_k; /* response-curve shape; 0.0 = linear, 2.0 = default */
    biosim_challenge_spec_t challenge;

    /* ── generation state ───────────────────────────────────────────────────── */
    uint32_t step;                  /* step index within the current generation */
    uint32_t gen;                   /* generation index (0-based) */
    float mutation_rate;            /* per-gene point-mutation probability */
    bool sexual_reproduction;       /* use two-parent crossover; default false */
    bool choose_parents_by_fitness; /* bias toward high-score survivors; default false */
    uint64_t gen_rng;               /* RNG state for generation-boundary operations */

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
    uint32_t n_barrier_ctrs;

    /* kill tracking — reset to 0 at each generation boundary */
    uint32_t kills;

    /* ── snapshot output session ─────────────────────────────────────────────
     * Configured via biosim_snapshot_session_open(); closed automatically by
     * biosim_sim_free(). snap_f == NULL means no active output session.     */
    FILE *snap_f;
    uint32_t snap_written_count;
    uint32_t snap_interval;
} biosim_sim_t;

/*
 * Lifecycle
 *
 * biosim_sim_create — zero *sim, read all configuration from *p, copy
 * *challenge, allocate all heap resources, place barriers on the grid,
 * and spawn the initial population.  barriers/n_barriers may be NULL/0.
 */
biosim_status_t biosim_sim_create(
    biosim_sim_t *sim,
    const biosim_params_t *p,
    const biosim_challenge_spec_t *challenge,
    const biosim_barrier_spec_t *barriers,
    uint32_t n_barriers
);

void biosim_sim_free(biosim_sim_t *sim);

/* Advance one agent by one step: evaluate sensors, run feedforward, apply
 * actions, and propose a move (desired_x/desired_y). Grid granting and kill
 * commits are deferred to biosim_sim_next_step. */
void biosim_sim_step_agent(biosim_sim_t *sim, uint32_t i);

/* Finalize the current step:
 *   1. Commit kills — agents with kill_marker set are marked dead and their
 *      grid cells cleared (mirrors GPU K2).
 *   2. Grant movement — each alive agent's proposed move is granted if the
 *      target cell is empty; first-come, first-served (mirrors GPU K3).
 *   3. Fade the signal layer (mirrors GPU K4).
 *   4. Run the per-step challenge hook and increment sim->step.
 */
void biosim_sim_next_step(biosim_sim_t *sim);

/*
 * Advance one generation: evaluate the challenge for all alive agents, write
 * the snapshot output session (if active), collect statistics, reproduce
 * survivors, recompile neural networks, and respawn the full population on the
 * grid.
 *
 * Reproduction mode is controlled by sim->sexual_reproduction (two-parent
 * crossover vs. single-parent copy) and sim->choose_parents_by_fitness
 * (score-biased vs. uniform random parent selection).
 *
 * After the call: sim->step and sim->kills are reset to 0 and sim->gen is
 * incremented. out receives the census taken from the just-completed generation.
 * Returns BIOSIM_ERR_NOMEM if any required allocation fails; on error the
 * generation counters are not advanced and out is not written.
 */
biosim_status_t biosim_sim_next_generation(biosim_sim_t *sim, struct biosim_census *out);

#endif /* BIOSIM_CORE_SIM_H */
