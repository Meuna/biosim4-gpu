/*
 * HOST-ONLY: includes biosim/core/sim.h and <stdio.h>.
 * Do NOT include from OpenCL kernel sources (.cl files).
 */
#ifndef BIOSIM_CORE_CENSUS_H
#define BIOSIM_CORE_CENSUS_H

#include "biosim/core/sim.h"
#include <stdint.h>
#include <stdio.h>

/*
 * Population census taken at the end of a generation, before reproduction.
 *
 * gen        — generation index (0-based), copied from sim->gen
 * population — total agent slots (sim->agents.population)
 * survivors  — agents that passed the challenge
 * kills      — agents killed by KILL_FORWARD during this generation
 *              (always 0 when enable_kill is false)
 *
 * Derived metrics (e.g. survival_rate = survivors / population) are computed
 * at display time and are not stored here.
 */
typedef struct biosim_census {
    uint32_t gen;
    uint32_t population;
    uint32_t survivors;
    uint32_t kills;
} biosim_census_t;

/*
 * Snapshot a census from the current sim state.
 *
 * n_survivors — number of agents that passed the challenge this generation
 * out         — census struct to fill; all fields are written
 *
 * Must be called before sim->kills is reset to 0.
 */
void biosim_census_take(const biosim_sim_t *sim, uint32_t n_survivors, biosim_census_t *out);

/* Print the census column header to stream. */
void biosim_census_print_header(FILE *stream);

/*
 * Print one census data row to stream.
 * survival_rate (surv%) is computed as survivors / population at print time.
 */
void biosim_census_print(FILE *stream, const biosim_census_t *c);

#endif /* BIOSIM_CORE_CENSUS_H */
