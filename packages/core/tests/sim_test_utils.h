#ifndef BIOSIM_SIM_TEST_UTILS_H
#define BIOSIM_SIM_TEST_UTILS_H

#include "biosim/core/agents.h"
#include "biosim/core/genome.h"
#include "biosim/core/grid.h"
#include "biosim/core/nnet.h"
#include "biosim/core/sim.h"

#include <stdint.h>

/* ── sim factories ──────────────────────────────────────────────────────────
 * Minimal sim: pop=4, 4x4 grid, genome_max_len=4, max_neurons=2.
 * Richer sim:  pop=64, 32x32 grid, 8 steps/gen, mutation enabled. */
biosim_sim_t sim_test_make_light(void);
biosim_sim_t sim_test_make_medium(void);

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
