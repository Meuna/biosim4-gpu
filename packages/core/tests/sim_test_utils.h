#ifndef BIOSIM_SIM_TEST_UTILS_H
#define BIOSIM_SIM_TEST_UTILS_H

#include "biosim/core/agents.h"
#include "biosim/core/genome.h"
#include "biosim/core/grid.h"
#include "biosim/core/nnet.h"
#include "biosim/core/sim.h"

#include <stdbool.h>
#include <stdint.h>

/* ── sim factory config ─────────────────────────────────────────────────────
 * Passed to sim_test_create().  Unspecified fields in a compound literal
 * zero-initialise (population_sensor_radius=0, mutation_rate=0, etc.) which
 * is safe for tests that do not exercise those features. */
typedef struct {
    uint32_t population;
    int32_t size_x;
    int32_t size_y;
    uint16_t genome_max_len;
    uint8_t max_neurons;
    uint8_t long_probe_dist;
    uint32_t steps_per_gen;
    int32_t population_sensor_radius;
    float mutation_rate;
    bool sexual_reproduction;
    bool choose_parents_by_fitness;
} sim_test_cfg_t;

/* Build and allocate a sim from an explicit config.  The challenge is set to
 * x_band [0, 1] with no mirror; tests that need a different challenge may
 * overwrite sim->challenge after the call. */
biosim_status_t sim_test_create(biosim_sim_t *sim, const sim_test_cfg_t *cfg);

/* ── named presets ──────────────────────────────────────────────────────────
 * Minimal sim: pop=4, 4x4 grid, genome_max_len=4, max_neurons=2.
 * Richer sim:  pop=64, 32x32 grid, 8 steps/gen, mutation enabled. */
biosim_status_t sim_test_make_light(biosim_sim_t *sim);
biosim_status_t sim_test_make_medium(biosim_sim_t *sim);

/* Run all steps of one full generation (does not call biosim_sim_next_generation). */
void sim_test_run_one_gen(biosim_sim_t *sim);

/* ── equality assertions ────────────────────────────────────────────────────
 * Each function calls TEST_ASSERT_* and must be invoked from a Unity test. */
void assert_agents_equal(const biosim_agents_t *a, const biosim_agents_t *b);
void assert_grid_equal(const biosim_grid_t *a, const biosim_grid_t *b);

/* Compares the first n_agents entries; uses each genome's own population as
 * the SoA stride, so works across mismatched population sizes. */
void assert_genome_slice_equal(const biosim_genome_t *a, const biosim_genome_t *b,
                               uint32_t n_agents);

/* Asserts equal populations then delegates to assert_genome_slice_equal. */
void assert_genome_equal(const biosim_genome_t *a, const biosim_genome_t *b);

void assert_nnet_equal(const biosim_nnet_t *a, const biosim_nnet_t *b);
void assert_sim_equal(const biosim_sim_t *a, const biosim_sim_t *b);

#endif /* BIOSIM_SIM_TEST_UTILS_H */
